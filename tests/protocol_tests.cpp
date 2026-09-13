#include "protocol.hpp"

#include <cassert>
#include <iostream>

namespace {

void feed(RingBuffer<512>& rx, const std::vector<uint8_t>& bytes) {
    for (uint8_t byte : bytes) {
        rx.push(byte);
    }
}

void test_round_trip() {
    proto::Frame in;
    in.seq = 7;
    in.cmd = proto::Command::GetVersion;

    RingBuffer<512> rx;
    feed(rx, proto::encode(in));

    auto out = proto::try_decode(rx);
    assert(out.has_value());
    assert(out->seq == 7);
    assert(out->cmd == proto::Command::GetVersion);
    assert(out->payload.empty());
}

void test_sticky_packets() {
    proto::Frame one{1, proto::Command::GetStatus, {}};
    proto::Frame two{2, proto::Command::SetPwm, proto::u8_payload(50)};

    auto bytes = proto::encode(one);
    auto more = proto::encode(two);
    bytes.insert(bytes.end(), more.begin(), more.end());

    RingBuffer<512> rx;
    feed(rx, bytes);

    auto a = proto::try_decode(rx);
    auto b = proto::try_decode(rx);

    assert(a.has_value());
    assert(b.has_value());
    assert(a->cmd == proto::Command::GetStatus);
    assert(b->cmd == proto::Command::SetPwm);
    uint8_t pwm = 0;
    assert(proto::payload_u8(b->payload, pwm));
    assert(pwm == 50);
}

void test_partial_packet() {
    proto::Frame in{9, proto::Command::SetLed, proto::text_payload("on")};
    auto bytes = proto::encode(in);

    RingBuffer<512> rx;
    for (std::size_t i = 0; i < bytes.size() - 1; ++i) {
        rx.push(bytes[i]);
    }
    assert(!proto::try_decode(rx).has_value());

    rx.push(bytes.back());
    auto out = proto::try_decode(rx);
    assert(out.has_value());
    assert(out->seq == 9);
}

void test_garbage_and_checksum_failure() {
    proto::Frame valid{3, proto::Command::GetSensor, {}};
    auto bad = proto::encode(proto::Frame{4, proto::Command::SetLed, proto::text_payload("bad")});
    bad.back() ^= 0x22;

    RingBuffer<512> rx;
    rx.push(0x00);
    rx.push(0x11);
    feed(rx, bad);
    feed(rx, proto::encode(valid));

    auto out = proto::try_decode(rx);
    while (!out.has_value() && rx.size() > 0) {
        out = proto::try_decode(rx);
    }

    assert(out.has_value());
    assert(out->cmd == proto::Command::GetSensor);
    assert(out->seq == 3);
}

} // namespace

int main() {
    test_round_trip();
    test_sticky_packets();
    test_partial_packet();
    test_garbage_and_checksum_failure();
    std::cout << "protocol_tests passed\n";
    return 0;
}
