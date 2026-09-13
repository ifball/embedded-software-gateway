#pragma once

#include <cstdint>
#include <vector>

class DeviceTransport {
public:
    virtual bool send_bytes(const std::vector<uint8_t>& data) = 0;
    virtual bool recv_bytes(std::vector<uint8_t>& data) = 0;
    virtual ~DeviceTransport() = default;
};

/*
 * Interview note:
 * The current gateway talks to device_sim by TCP so the whole project can be
 * reproduced on a laptop. A production embedded Linux version should provide:
 *
 *   TcpDeviceTransport    - current simulator/backend testing path
 *   SerialDeviceTransport - termios backend for /dev/ttyUSB0
 *
 * Keeping this interface small lets protocol parsing and command dispatch stay
 * unchanged when the device transport changes.
 */
