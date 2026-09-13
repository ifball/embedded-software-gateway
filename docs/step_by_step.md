# Step By Step Guide

This guide teaches you how to rebuild the embedded software gateway project.
The focus is software, not hardware.

## Stage 0: Project Goal

You are building:

```text
PC client -> C++ gateway -> device endpoint
```

The gateway is the main project. It receives human-readable commands from a PC
client, converts them into binary protocol frames, forwards them to a device
endpoint and returns the device response.

## Stage 1: Run The Finished Demo First

Build:

```powershell
cd C:\Users\20934\Desktop\embedded_software_gateway_project
cmake -S . -B build
cmake --build build --config Release
```

Run three terminals:

```powershell
.\build\Release\device_sim.exe
```

```powershell
.\build\Release\gateway.exe
```

```powershell
python .\pc_client\client.py
```

Try:

```text
status
sensor
version
led on
pwm 50
pwm 101
stats
reset_error
```

At this point, you have a runnable embedded software communication chain.

## Stage 2: Understand RingBuffer

Read:

```text
common/ring_buffer.hpp
```

Why it exists:

TCP and UART are byte streams. One `recv()` may receive:

- half a frame
- exactly one frame
- multiple frames
- garbage bytes before a valid frame

RingBuffer stores bytes until the parser can extract a complete frame.

You must understand:

```text
push()  writes one byte
pop()   removes one byte
peek()  reads without removing
drop()  skips garbage or consumed frame bytes
```

Interview answer:

```text
我没有假设一次接收就是一帧，而是先把字节放进RingBuffer，再由协议解析器按帧头和长度逐帧解析。
```

## Stage 3: Understand The Protocol

Read:

```text
common/protocol.hpp
common/protocol.cpp
```

Frame:

```text
AA 55 LEN SEQ CMD PAYLOAD CHECKSUM
```

Why each field exists:

```text
AA 55     find frame boundary
LEN       handle sticky/partial packets
SEQ       match request and response
CMD       dispatch command
PAYLOAD   carry arguments
CHECKSUM  detect corrupted data
```

Implemented feature:

```text
GET_VERSION = 0x06
```

The device returns:

```text
version=1.0.0
```

## Stage 4: Understand The Gateway

Read:

```text
gateway/main.cpp
```

The gateway does four jobs:

```text
1. Accept PC client connection.
2. Parse text command.
3. Encode binary device frame.
4. Decode device response and return readable text.
```

Important functions:

```text
parse_command()
send_all()
read_device_response()
proto::encode()
proto::try_decode()
```

Implemented feature:

```text
[gateway] seq=3 cmd=SET_PWM cost_ms=2
```

You can also query:

```text
stats
```

## Stage 5: Understand The Device Endpoint

Read:

```text
device_sim/main.cpp
```

This is not the main resume point. It is a controllable endpoint for testing
the gateway without buying hardware.

It simulates:

```text
status value
telemetry value
control output
error counter
```

Later, this endpoint can become:

```text
serial device
STM32 board
RTOS terminal
industrial controller
sensor hub
```

## Stage 6: Understand The Added Feature

The project already supports:

```text
version
```

It touches these files:

```text
common/protocol.hpp
common/protocol.cpp
gateway/main.cpp
device_sim/main.cpp
README.md
```

Acceptance:

```text
pc> version
ACK seq=... version=1.0.0
```

## Stage 7: Understand Binary Payload

`pwm 50` sends binary payload:

```text
payload[0] = 50
```

Why this matters:

Binary payloads are smaller, clearer and closer to real embedded protocols.

## Stage 8: Add Software Engineering Evidence

The project already includes protocol tests. You should still add a real run
log before resume delivery:

```text
test_log.md
logs/sample_run.log
```

Record:

```text
normal commands
bad commands
invalid payload
device timeout
checksum failure
30-minute stability test
```

## Stage 9: Optional Serial Backend

After the software version is stable, define an interface:

```text
class DeviceTransport {
public:
    virtual bool send_bytes(const std::vector<uint8_t>& data) = 0;
    virtual bool recv_bytes(std::vector<uint8_t>& data) = 0;
    virtual ~DeviceTransport() = default;
};
```

Then implement:

```text
TcpDeviceTransport     current simulator version
SerialDeviceTransport  future /dev/ttyUSB0 or COM port version
```

This is the clean way to mention hardware without turning the project into a
hardware project.

## Stage 10: Interview Explanation

Use this version:

```text
我做的是一个嵌入式Linux应用层通信网关。PC端发文本命令，网关把命令转成自定义二进制协议帧，
通过设备通信后端转发给设备端。协议里有帧头、长度、序号、命令字、payload和checksum，
接收侧用RingBuffer处理半包、粘包和垃圾字节。这个项目重点展示的是C/C++模块化、
socket通信、协议解析、命令分发、错误处理和可测试性。设备端现在是模拟器，后续可以替换为串口设备。
```
