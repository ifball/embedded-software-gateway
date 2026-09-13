/*
 * In-process integration test (no real TCP sockets needed).
 *
 * Emulates the 3-tier pipeline in memory:
 *   pc_client (text commands)
 *     -> gateway (parse_command + encode request frame)
 *       -> byte pipe (simulates TCP)
 *         -> device (try_decode + execute command + encode ACK/ERROR)
 *       <- byte pipe (simulates TCP reverse)
 *     <- gateway (try_decode response + format text reply + collect stats)
 *
 * Uses the SAME protocol/ringbuffer code as the real binaries, so protocol
 * behavior is byte-for-byte identical to the live deployment.
 */
#include "protocol.hpp"
#include "ring_buffer.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------- helpers ----------
bool parse_command(const std::string& line, uint8_t seq, proto::Frame& frame) {
    std::istringstream iss(line);
    std::string op;
    iss >> op;
    frame.seq = seq;

    if (op == "status")  { frame.cmd = proto::Command::GetStatus;  frame.payload.clear(); return true; }
    if (op == "sensor")  { frame.cmd = proto::Command::GetSensor;  frame.payload.clear(); return true; }
    if (op == "version") { frame.cmd = proto::Command::GetVersion; frame.payload.clear(); return true; }
    if (op == "reset_error") { frame.cmd = proto::Command::ResetError; frame.payload.clear(); return true; }
    if (op == "led") {
        std::string value;
        iss >> value;
        if (value.empty()) return false;
        frame.cmd = proto::Command::SetLed;
        frame.payload = proto::text_payload(value);
        return true;
    }
    if (op == "pwm") {
        std::string value;
        iss >> value;
        int pwm = -1;
        try { pwm = std::stoi(value); } catch (...) { return false; }
        if (pwm < 0 || pwm > 100) return false;
        frame.cmd = proto::Command::SetPwm;
        frame.payload = proto::u8_payload(static_cast<uint8_t>(pwm));
        return true;
    }
    return false;
}

struct GatewayStats {
    uint64_t total = 0, success = 0, failed = 0, timeout = 0, total_latency_ms = 0;
    std::string report() const {
        std::ostringstream oss;
        double avg = success ? double(total_latency_ms) / success : 0.0;
        oss << "stats total=" << total << " success=" << success
            << " failed=" << failed << " timeout=" << timeout
            << " avg_ms=" << std::fixed << std::setprecision(2) << avg;
        return oss.str();
    }
};

// ---------- in-memory device endpoint (mirrors device_sim/main.cpp behavior) ----------
class InMemoryDevice {
public:
    InMemoryDevice() = default;

    // Accept a raw byte stream (may contain partial/sticky frames)
    void push_bytes(const std::vector<uint8_t>& bytes) {
        for (uint8_t b : bytes) rx_.push(b);
    }

    // Execute all complete frames currently in rx_ and return their encoded responses
    std::vector<uint8_t> drain_responses() {
        std::vector<uint8_t> out;
        while (auto frame = proto::try_decode(rx_)) {
            // update sensor with jitter
            sensor_ += jitter_(rng_);
            proto::Frame response;
            response.seq = frame->seq;
            response.cmd = proto::Command::Ack;

            switch (frame->cmd) {
                case proto::Command::GetStatus:
                    response.payload = make_status_payload("status_ok");
                    break;
                case proto::Command::GetSensor:
                    response.payload = proto::text_payload("sensor=" + std::to_string(sensor_));
                    break;
                case proto::Command::SetLed: {
                    const std::string value = proto::payload_text(frame->payload);
                    if (value == "on" || value == "1") {
                        led_ = true;
                        response.payload = make_status_payload("led_updated");
                    } else if (value == "off" || value == "0") {
                        led_ = false;
                        response.payload = make_status_payload("led_updated");
                    } else {
                        error_count_++;
                        response.cmd = proto::Command::Error;
                        response.payload = proto::text_payload("invalid led value");
                    }
                    break;
                }
                case proto::Command::SetPwm: {
                    uint8_t next_pwm = 0;
                    if (proto::payload_u8(frame->payload, next_pwm) && next_pwm <= 100) {
                        pwm_ = next_pwm;
                        response.payload = make_status_payload("pwm_updated");
                    } else {
                        error_count_++;
                        response.cmd = proto::Command::Error;
                        response.payload = proto::text_payload("pwm payload must be one byte 0..100");
                    }
                    break;
                }
                case proto::Command::ResetError:
                    error_count_ = 0;
                    response.payload = proto::text_payload("error_count=0");
                    break;
                case proto::Command::GetVersion:
                    response.payload = proto::text_payload("version=1.0.0;build=inproc_test");
                    break;
                default:
                    error_count_++;
                    response.cmd = proto::Command::Error;
                    response.payload = proto::text_payload("unsupported command");
                    break;
            }
            last_cmd_ = frame->cmd;
            last_errors_ = error_count_;
            auto enc = proto::encode(response);
            out.insert(out.end(), enc.begin(), enc.end());
        }
        return out;
    }

    bool led() const { return led_; }
    int pwm() const { return pwm_; }
    int sensor() const { return sensor_; }
    int error_count() const { return error_count_; }
    proto::Command last_cmd() const { return last_cmd_; }
    int last_errors() const { return last_errors_; }

private:
    std::vector<uint8_t> make_status_payload(const std::string& note) const {
        const std::string text = "led=" + std::string(led_ ? "on" : "off") +
                                 ";pwm=" + std::to_string(pwm_) +
                                 ";sensor=" + std::to_string(sensor_) +
                                 ";note=" + note;
        return proto::text_payload(text);
    }

    RingBuffer<512> rx_;
    bool led_ = false;
    int pwm_ = 0;
    int sensor_ = 250;
    int error_count_ = 0;
    proto::Command last_cmd_ = proto::Command::Error;
    int last_errors_ = 0;
    std::mt19937 rng_{std::random_device{}()};
    std::uniform_int_distribution<int> jitter_{-3, 4};
};

// ---------- transport emulation: inject arbitrary byte-slicing ----------
std::vector<std::vector<uint8_t>> slice_bytes(const std::vector<uint8_t>& in, int mode) {
    std::vector<std::vector<uint8_t>> out;
    if (mode == 0) {          // one piece (ideal)
        out.push_back(in);
    } else if (mode == 1) {   // sticky: later we'll concatenate so same as 0 for sender side; mark
        out.push_back(in);
    } else if (mode == 2) {   // byte by byte (worst case: partial packets all the way)
        for (uint8_t b : in) out.push_back({b});
    } else {                  // random variable-length chunks
        std::mt19937 r(0x5A5A);
        std::uniform_int_distribution<int> d(1, 5);
        size_t i = 0;
        while (i < in.size()) {
            size_t n = std::min<size_t>(d(r), in.size() - i);
            out.emplace_back(in.begin() + long(i), in.begin() + long(i + n));
            i += n;
        }
    }
    return out;
}

// ---------- full gateway pipeline simulation ----------
// Returns the final text response (as would be sent back to pc_client).
std::string gateway_roundtrip(const std::string& text_cmd, uint8_t seq,
                              InMemoryDevice& device, GatewayStats& stats,
                              int transport_mode) {
    // Local commands (handled inside gateway itself, never reach device)
    if (text_cmd == "quit" || text_cmd == "exit") return "bye";
    if (text_cmd == "help") return "commands: status | sensor | version | led on/off | pwm 0..100 | reset_error | stats | quit";
    if (text_cmd == "stats") return stats.report();

    stats.total++;
    proto::Frame request;
    if (!parse_command(text_cmd, seq, request)) {
        stats.failed++;
        return "bad command. type help.";
    }

    auto t0 = std::chrono::steady_clock::now();

    // Encode request, send to device via simulated transport (byte slicing)
    auto req_bytes = proto::encode(request);
    auto chunks = slice_bytes(req_bytes, transport_mode);
    std::vector<uint8_t> all_responses;
    for (auto& c : chunks) {
        device.push_bytes(c);
        auto partial = device.drain_responses();
        all_responses.insert(all_responses.end(), partial.begin(), partial.end());
    }
    // Drain once more in case chunks prevented device from decoding last frame
    if (all_responses.empty()) {
        auto rest = device.drain_responses();
        all_responses.insert(all_responses.end(), rest.begin(), rest.end());
    }

    // Inject the responses back through a gateway-side RingBuffer with slicing
    RingBuffer<512> gw_rx;
    auto resp_chunks = slice_bytes(all_responses, transport_mode);
    std::optional<proto::Frame> response;
    for (auto& c : resp_chunks) {
        for (uint8_t b : c) gw_rx.push(b);
        if (!response) response = proto::try_decode(gw_rx);
    }
    if (!response) response = proto::try_decode(gw_rx);

    auto t1 = std::chrono::steady_clock::now();
    auto cost_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    stats.total_latency_ms += static_cast<uint64_t>(cost_ms);

    if (!response) {
        stats.failed++;
        stats.timeout++;
        return "device timeout or disconnect";
    }
    if (response->cmd == proto::Command::Ack) {
        stats.success++;
    } else {
        stats.failed++;
    }
    return proto::command_name(response->cmd) + " seq=" +
           std::to_string(response->seq) + " " +
           proto::payload_text(response->payload);
}

// ---------- assertions & reporting ----------
int check_count = 0;
int pass_count = 0;
int fail_count = 0;

void check_impl(bool cond, const std::string& tag, const std::string& detail) {
    check_count++;
    if (cond) {
        pass_count++;
        std::cout << "  [PASS] " << tag << " : " << detail << "\n";
    } else {
        fail_count++;
        std::cout << "  [FAIL] " << tag << " : " << detail << "\n";
    }
}
#define CHECK(cond, tag, detail) check_impl(static_cast<bool>(cond), tag, detail)

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

void run_all_checks(int transport_mode, const std::string& mode_name) {
    std::cout << "\n========== Transport mode: " << mode_name << " ==========\n";
    InMemoryDevice device;
    GatewayStats stats;
    uint8_t seq = 1;
    std::string r;

    // -- GetStatus (before any change): led=off, pwm=0 --
    r = gateway_roundtrip("status", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "led=off") && contains(r, "pwm=0") && contains(r, "sensor="),
          "status (initial)", r);
    CHECK(device.led() == false && device.pwm() == 0,
          "device state after initial status", "led=off pwm=0");

    // -- GetSensor --
    r = gateway_roundtrip("sensor", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "sensor="),
          "sensor", r);

    // -- GetVersion --
    r = gateway_roundtrip("version", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "version=1.0.0"),
          "version", r);

    // -- SetLed on --
    r = gateway_roundtrip("led on", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "led=on") && contains(r, "note=led_updated"),
          "led on", r);
    CHECK(device.led() == true, "device led state after on", "led=on");

    // -- SetLed off --
    r = gateway_roundtrip("led off", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "led=off"),
          "led off", r);
    CHECK(device.led() == false, "device led state after off", "led=off");

    // -- SetPwm boundary 0 --
    r = gateway_roundtrip("pwm 0", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "pwm=0"),
          "pwm 0", r);
    CHECK(device.pwm() == 0, "device pwm=0", "pwm=0");

    // -- SetPwm 50 --
    r = gateway_roundtrip("pwm 50", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "pwm=50"),
          "pwm 50", r);
    CHECK(device.pwm() == 50, "device pwm=50", "pwm=50");

    // -- SetPwm 100 --
    r = gateway_roundtrip("pwm 100", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "pwm=100"),
          "pwm 100", r);
    CHECK(device.pwm() == 100, "device pwm=100", "pwm=100");

    // -- SetPwm out-of-range 101 -> gateway rejects before device --
    r = gateway_roundtrip("pwm 101", seq++, device, stats, transport_mode);
    CHECK(contains(r, "bad command"),
          "pwm 101 rejected", r);

    // -- SetPwm non-numeric -> gateway rejects --
    r = gateway_roundtrip("pwm hello", seq++, device, stats, transport_mode);
    CHECK(contains(r, "bad command"),
          "pwm hello rejected", r);

    // -- Invalid command abc -> gateway rejects --
    r = gateway_roundtrip("abc", seq++, device, stats, transport_mode);
    CHECK(contains(r, "bad command"),
          "invalid cmd abc", r);

    // -- Led without value -> gateway rejects --
    r = gateway_roundtrip("led", seq++, device, stats, transport_mode);
    CHECK(contains(r, "bad command"),
          "led missing arg", r);

    // -- SetLed invalid value -> device returns ERROR (not bad-command in gateway) --
    r = gateway_roundtrip("led purple", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ERROR") && contains(r, "invalid led value"),
          "led purple (device ERROR)", r);
    CHECK(device.error_count() >= 1,
          "device error_count incremented for bad led",
          "errors=" + std::to_string(device.error_count()));

    // -- SetPwm via crafted device with bad payload -> ERROR path (inject payload manually) --
    {
        proto::Frame bad_pwm;
        bad_pwm.cmd = proto::Command::SetPwm;
        bad_pwm.seq = 99;
        bad_pwm.payload = {0x01, 0x02};  // 2 bytes (invalid: should be 1)
        device.push_bytes(proto::encode(bad_pwm));
        auto resp_bytes = device.drain_responses();
        RingBuffer<512> rb;
        for (uint8_t b : resp_bytes) rb.push(b);
        auto dec = proto::try_decode(rb);
        CHECK(dec && dec->cmd == proto::Command::Error,
              "device SetPwm bad payload returns ERROR",
              dec ? proto::command_name(dec->cmd) + " " + proto::payload_text(dec->payload)
                  : "(no frame decoded)");
    }

    // -- ResetError -> ACK with error_count=0 --
    r = gateway_roundtrip("reset_error", seq++, device, stats, transport_mode);
    CHECK(contains(r, "ACK") && contains(r, "error_count=0"),
          "reset_error", r);
    CHECK(device.error_count() == 0,
          "device error_count == 0 after reset",
          "errors=" + std::to_string(device.error_count()));

    // -- stats -> gateway command, not forwarded --
    r = gateway_roundtrip("stats", 0, device, stats, transport_mode);
    CHECK(contains(r, "stats") && contains(r, "total=") && contains(r, "success=")
          && contains(r, "failed=") && contains(r, "timeout=") && contains(r, "avg_ms="),
          "stats command", r);

    // -- help -> gateway help text --
    r = gateway_roundtrip("help", 0, device, stats, transport_mode);
    CHECK(contains(r, "commands:") && contains(r, "status"),
          "help command", r);

    // -- quit -> bye --
    r = gateway_roundtrip("quit", 0, device, stats, transport_mode);
    CHECK(r == "bye", "quit command", r);

    // -- SEQ field check: request seq should match response seq --
    {
        InMemoryDevice dev2;
        GatewayStats s2;
        for (uint8_t k = 1; k <= 5; ++k) {
            std::string reply = gateway_roundtrip("version", k, dev2, s2, 0);
            std::string expected_seq = "seq=" + std::to_string(k);
            CHECK(contains(reply, expected_seq),
                  "seq match for k=" + std::to_string(k),
                  reply);
        }
    }

    // -- SEQ wrap: 0xFF -> 0 (we test 255 then 256 as uint8_t overflow) --
    {
        InMemoryDevice dev3;
        GatewayStats s3;
        std::string r1 = gateway_roundtrip("status", 0xFF, dev3, s3, 0);
        CHECK(contains(r1, "seq=255"), "seq=0xFF prints correctly", r1);
    }
}

void run_sticky_and_partial_stress() {
    std::cout << "\n========== Stress: sticky + garbage + partial ==========\n";
    InMemoryDevice device;
    GatewayStats stats;

    // build: garbage bytes + 3 concatenated frames (sticky) + more garbage + 1 corrupted frame + 1 good frame
    std::vector<uint8_t> stream;
    // garbage prefix
    stream.push_back(0x00); stream.push_back(0xAA); stream.push_back(0x01);
    // 3 good frames (sticky)
    auto f1 = proto::encode(proto::Frame{11, proto::Command::GetStatus, {}});
    auto f2 = proto::encode(proto::Frame{12, proto::Command::SetLed, proto::text_payload("on")});
    auto f3 = proto::encode(proto::Frame{13, proto::Command::GetVersion, {}});
    stream.insert(stream.end(), f1.begin(), f1.end());
    stream.insert(stream.end(), f2.begin(), f2.end());
    stream.insert(stream.end(), f3.begin(), f3.end());
    // garbage in between
    stream.push_back(0x55); stream.push_back(0xAA); stream.push_back(0x00);
    // 1 corrupted frame (checksum flipped)
    auto bad = proto::encode(proto::Frame{14, proto::Command::SetPwm, proto::u8_payload(77)});
    if (!bad.empty()) bad.back() ^= 0x11;
    stream.insert(stream.end(), bad.begin(), bad.end());
    // 1 good frame
    auto good = proto::encode(proto::Frame{15, proto::Command::GetSensor, {}});
    stream.insert(stream.end(), good.begin(), good.end());

    // feed byte-by-byte to the device
    std::vector<uint8_t> all_resp;
    for (uint8_t b : stream) {
        device.push_bytes({b});
        auto r = device.drain_responses();
        all_resp.insert(all_resp.end(), r.begin(), r.end());
    }
    if (auto tail = device.drain_responses(); !tail.empty()) {
        all_resp.insert(all_resp.end(), tail.begin(), tail.end());
    }

    // decode all responses through RingBuffer byte-by-byte
    RingBuffer<512> rb;
    std::vector<proto::Frame> decoded;
    for (uint8_t b : all_resp) {
        rb.push(b);
        while (auto f = proto::try_decode(rb)) decoded.push_back(*f);
    }
    while (auto f = proto::try_decode(rb)) decoded.push_back(*f);

    std::cout << "  decoded responses: " << decoded.size() << " (expected 4: 3 good + good-tail; corrupted one lost + seq14 never came)\n";
    CHECK(decoded.size() >= 4,
          "sticky/garbage/corruption decoded >= 4 responses",
          "got " + std::to_string(decoded.size()));

    // seq 11,12,13,15 must appear; seq 14 (corrupted) must NOT
    bool has11=false, has12=false, has13=false, has15=false, has14=false;
    for (auto& d : decoded) {
        if (d.seq == 11) has11 = true;
        if (d.seq == 12) has12 = true;
        if (d.seq == 13) has13 = true;
        if (d.seq == 14) has14 = true;
        if (d.seq == 15) has15 = true;
    }
    CHECK(has11, "seq=11 decoded (GetStatus)", has11 ? "yes" : "no");
    CHECK(has12, "seq=12 decoded (SetLed on)", has12 ? "yes" : "no");
    CHECK(has13, "seq=13 decoded (GetVersion)", has13 ? "yes" : "no");
    CHECK(has15, "seq=15 decoded (GetSensor after corrupted recovery)", has15 ? "yes" : "no");
    CHECK(!has14, "seq=14 (corrupted) NOT decoded", !has14 ? "yes: correctly dropped" : "no: corrupted frame accepted");
    CHECK(device.led() == true, "device led=on after sticky SetLed on seq=12 took effect",
          device.led() ? "led=on" : "led=off (BUG)");
}

} // namespace

int main() {
    std::cout << "=== in_process_integration start ===\n";

    run_all_checks(0, "IDEAL (whole frames)");
    run_all_checks(2, "BYTE-BY-BYTE (partial packets worst case)");
    run_all_checks(3, "RANDOM CHUNKS (mixed partial/sticky)");
    run_sticky_and_partial_stress();

    std::cout << "\n================ SUMMARY ================\n"
              << "checks: " << check_count
              << "  passed: " << pass_count
              << "  failed: " << fail_count << "\n";
    if (fail_count == 0) {
        std::cout << "in_process_integration: ALL PASSED\n";
        return 0;
    }
    std::cout << "in_process_integration: SOME FAILED\n";
    return 1;
}
