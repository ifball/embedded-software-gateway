#pragma once

#include "ring_buffer.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace proto {

constexpr uint8_t SOF1 = 0xAA;
constexpr uint8_t SOF2 = 0x55;
constexpr std::size_t MAX_PAYLOAD = 64;

enum class Command : uint8_t {
    GetStatus = 0x01,
    GetSensor = 0x02,
    SetLed = 0x03,
    SetPwm = 0x04,
    ResetError = 0x05,
    GetVersion = 0x06,
    Ack = 0x80,
    Error = 0x81,
};

struct Frame {
    uint8_t seq = 0;
    Command cmd = Command::Error;
    std::vector<uint8_t> payload;
};

std::vector<uint8_t> encode(const Frame& frame);
std::optional<Frame> try_decode(RingBuffer<512>& rx);
uint8_t checksum(const std::vector<uint8_t>& bytes, std::size_t begin, std::size_t end);
std::string command_name(Command cmd);

std::vector<uint8_t> text_payload(const std::string& text);
std::string payload_text(const std::vector<uint8_t>& payload);
std::vector<uint8_t> u8_payload(uint8_t value);
bool payload_u8(const std::vector<uint8_t>& payload, uint8_t& value);

} // namespace proto
