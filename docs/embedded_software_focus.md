# Embedded Software Focus

Use this file to keep the project story aligned with embedded software
internship roles.

## Say This

```text
这是一个嵌入式 Linux 应用层通信网关项目。
```

```text
重点是 C/C++、socket、协议帧、RingBuffer、命令分发、错误处理、日志和测试。
```

```text
设备端先用模拟器，方便稳定复现；后续可以替换成串口设备或 RTOS 终端。
```

## Do Not Say This First

```text
这是一个 STM32 硬件项目。
```

```text
我做了 LED、PWM、传感器。
```

These are only endpoint examples. The stronger story is the gateway and
protocol software.

## Skill Mapping

| Project Part | Internship Skill |
| --- | --- |
| `gateway/main.cpp` | Linux C/C++ application development |
| TCP command server | socket programming |
| `common/protocol.cpp` | protocol design and parsing |
| `common/ring_buffer.hpp` | byte stream buffering |
| checksum failure handling | error-path design |
| command dispatcher | module boundary and API design |
| device simulator | testability without hardware |
| optional serial backend | embedded device communication |

## Resume Name Options

Use one of these:

```text
基于 C/C++ 的嵌入式 Linux 设备通信网关
```

```text
嵌入式 Linux 设备协议网关与命令分发系统
```

```text
C/C++ 设备通信协议栈与 TCP 网关项目
```

## Best Resume Bullet

```text
基于 C/C++ 实现嵌入式 Linux 设备通信网关，支持 TCP 命令接入、自定义二进制协议封装、
RingBuffer 字节流解析、设备状态查询、控制命令转发、二进制 payload、校验失败处理、
超时错误统计、响应耗时统计和协议单元测试。
```

## Interview Deep-Dive Points

Prepare to explain:

- Why TCP/UART are byte streams.
- Why one `recv()` is not equal to one frame.
- How `LEN` solves sticky and partial packets.
- Why RingBuffer is better than parsing directly from temporary buffers.
- What happens when checksum fails.
- Where timeout should be handled.
- How to replace simulator with serial transport.
- How to test without real hardware.
