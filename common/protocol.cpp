#include "protocol.hpp"

#include <sstream>

namespace proto {

uint8_t checksum(const std::vector<uint8_t>& bytes, std::size_t begin, std::size_t end) {
    uint8_t value = 0;
    for (std::size_t i = begin; i < end; ++i) {
        value ^= bytes[i];
    }
    return value;
}

std::vector<uint8_t> encode(const Frame& frame) {
    std::vector<uint8_t> out;
    const std::size_t payload_len = frame.payload.size();
    const uint8_t len = static_cast<uint8_t>(1 + payload_len);

    out.reserve(6 + payload_len);
    out.push_back(SOF1);
    out.push_back(SOF2);
    out.push_back(len);
    out.push_back(frame.seq);
    out.push_back(static_cast<uint8_t>(frame.cmd));
    out.insert(out.end(), frame.payload.begin(), frame.payload.end());
    out.push_back(checksum(out, 2, out.size()));
    return out;
}

std::optional<Frame> try_decode(RingBuffer<512>& rx) {
    while (rx.size() >= 2) {
        uint8_t b0 = 0;
        uint8_t b1 = 0;
        rx.peek(0, b0);
        rx.peek(1, b1);
        if (b0 == SOF1 && b1 == SOF2) {
            break;
        }
        rx.drop(1);
    }

    if (rx.size() < 6) {
        return std::nullopt;
    }

    uint8_t len = 0;
    rx.peek(2, len);
    if (len == 0 || len > MAX_PAYLOAD + 1) {
        rx.drop(1);
        return std::nullopt;
    }

    const std::size_t total = 2 + 1 + 1 + len + 1;
    if (rx.size() < total) {
        return std::nullopt;
    }

    std::vector<uint8_t> raw(total);
    for (std::size_t i = 0; i < total; ++i) {
        rx.peek(i, raw[i]);
    }

    const uint8_t expected = checksum(raw, 2, total - 1);
    if (expected != raw[total - 1]) {
        rx.drop(1);
        return std::nullopt;
    }

    Frame frame;
    frame.seq = raw[3];
    frame.cmd = static_cast<Command>(raw[4]);
    frame.payload.assign(raw.begin() + 5, raw.end() - 1);
    rx.drop(total);
    return frame;
}

std::string command_name(Command cmd) {
    switch (cmd) {
        case Command::GetStatus: return "GET_STATUS";
        case Command::GetSensor: return "GET_SENSOR";
        case Command::SetLed: return "SET_LED";
        case Command::SetPwm: return "SET_PWM";
        case Command::ResetError: return "RESET_ERROR";
        case Command::GetVersion: return "GET_VERSION";
        case Command::Ack: return "ACK";
        case Command::Error: return "ERROR";
        default: break;
    }
    std::ostringstream oss;
    oss << "UNKNOWN(0x" << std::hex << static_cast<int>(cmd) << ")";
    return oss.str();
}

std::vector<uint8_t> text_payload(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::string payload_text(const std::vector<uint8_t>& payload) {
    return std::string(payload.begin(), payload.end());
}

std::vector<uint8_t> u8_payload(uint8_t value) {
    return {value};
}

bool payload_u8(const std::vector<uint8_t>& payload, uint8_t& value) {
    if (payload.size() != 1) {
        return false;
    }
    value = payload[0];
    return true;
}

} // namespace proto
