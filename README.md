# Embedded Software Gateway Project

Chinese project name:

> 基于 C/C++ 的嵌入式 Linux 设备通信网关

This project focuses on embedded Linux application-layer development, protocol
parsing, byte-stream buffering, gateway forwarding, error handling and
testability.

The desktop demo runs this chain:

```text
PC command client
  -> TCP text command
C++ embedded software gateway
  -> custom binary protocol
device endpoint simulator
```

The device endpoint is a simulator first. Later it can be replaced by a real
serial device, MCU board, sensor module, motor controller or RTOS terminal.
The core project is still software: protocol, gateway, buffering, concurrency,
logging, testing and debugging.

## Why This Fits Embedded Software

Embedded software engineers often work at the boundary between an operating
system and devices. This project demonstrates that boundary without requiring
you to design hardware.

It proves:

- C/C++ modular development
- Linux-style gateway architecture
- TCP socket programming
- Device command forwarding
- RingBuffer byte-stream parsing
- Custom binary protocol design
- Sticky/partial packet handling
- Checksum validation
- Binary payload handling for `SET_PWM`
- Gateway statistics: total/success/failed/timeout/average latency
- Timeout and error-path handling
- Protocol tests for round-trip, sticky packet, partial packet and checksum failure
- Clear README and test plan

Optional hardware migration is only an extension. The core is:

```text
embedded Linux gateway + protocol parser + device communication backend
```

## Architecture

```text
+------------------+        TCP text command        +----------------------+
| PC client        | -----------------------------> | C++ gateway          |
| client.py        | <----------------------------- | command dispatcher   |
+------------------+         text response          +----------+-----------+
                                                              |
                                                              | binary frame
                                                              v
                                                   +----------------------+
                                                   | device endpoint      |
                                                   | simulator now        |
                                                   | serial/RTOS later    |
                                                   +----------------------+
```

## Directory Structure

```text
common/
  protocol.hpp/.cpp        protocol encode/decode
  ring_buffer.hpp          byte-stream buffer

gateway/
  main.cpp                 C++ gateway and command dispatcher
  include/
    device_transport.hpp   transport abstraction for TCP/serial backend

device_sim/
  main.cpp                 software device endpoint simulator

pc_client/
  client.py                interactive command client

firmware_stm32/
  protocol_port.*          optional MCU/RTOS migration reference
  freertos_tasks_sketch.c  optional task split reference

docs/
  step_by_step.md          rebuild guide
  test_plan.md
```

## Protocol

```text
AA 55 LEN SEQ CMD PAYLOAD CHECKSUM
```

- `AA 55`: frame header
- `LEN`: `CMD + PAYLOAD` length
- `SEQ`: request sequence number
- `CMD`: command byte
- `PAYLOAD`: command data
- `CHECKSUM`: XOR of `LEN`, `SEQ`, `CMD`, `PAYLOAD`

Commands:

| Command | Value | Meaning |
| --- | ---: | --- |
| GET_STATUS | 0x01 | Read device status |
| GET_SENSOR | 0x02 | Read simulated telemetry value |
| SET_LED | 0x03 | Set a boolean output state |
| SET_PWM | 0x04 | Set a percentage control value |
| GET_VERSION | 0x06 | Read endpoint version |
| RESET_ERROR | 0x05 | Clear error counter |
| ACK | 0x80 | Successful response |
| ERROR | 0x81 | Error response |

In a more software-oriented explanation, `LED/PWM/sensor` can be renamed as:

```text
output switch / control value / telemetry value
```

That keeps the same engineering idea while avoiding a hardware-heavy story.

## Build

Windows:

```powershell
cd embedded_software_gateway_project
cmake -S . -B build
cmake --build build --config Release
.\build\Release\protocol_tests.exe
python .\tools_protocol_selftest.py
```

Linux:

```bash
cd embedded_software_gateway_project
cmake -S . -B build
cmake --build build
./build/protocol_tests
python3 ./tools_protocol_selftest.py
```

## Run

Open three terminals.

Terminal 1:

```powershell
.\build\Release\device_sim.exe
```

Terminal 2:

```powershell
.\build\Release\gateway.exe
```

Terminal 3:

```powershell
python .\pc_client\client.py
```

Try:

```text
status
sensor
version
led on
led off
pwm 50
pwm 101
stats
reset_error
quit
```

## How To Make It Yours

This version includes:

- `GET_VERSION` command
- binary one-byte payload for `SET_PWM`
- gateway response-time logging
- gateway runtime statistics via `stats`
- protocol tests
- device transport abstraction note

Run it locally and write your own `test_log.md` with real output from your
machine.
