# Resume Delivery Checklist

Do not put the project on your resume until most items are checked.

## Must Have

- The project builds from a clean directory.
- `device_sim`, `gateway` and `client.py` can run together.
- README explains build, run, protocol and commands.
- `status`, `sensor`, `led on/off`, `pwm 0..100` work.
- `version` works.
- `stats` reports total/success/failed/timeout/avg_ms.
- Invalid command and invalid payload are tested.
- `SET_PWM` uses one-byte binary payload.
- You wrote `test_log.md` with real command output.
- You can draw `docs/gateway_state_machine.md` by hand.
- You can explain RingBuffer without reading code.
- You can explain why `LEN` is needed in a byte-stream protocol.
- You can explain how checksum failure is handled.

## Better Version

- Add device reconnect state machine.
- Add CRC16 instead of XOR.
- Add `TcpDeviceTransport` implementation class.
- Add `SerialDeviceTransport` for `/dev/ttyUSB0`.
- Add CI workflow if you upload to GitHub.

## Resume Wording

Use:

```text
嵌入式 Linux 设备通信网关
```

Avoid leading with:

```text
STM32硬件项目
```

## 30-Second Pitch

```text
我做了一个嵌入式Linux应用层通信网关。PC端通过TCP发送命令，网关把命令封装成自定义二进制协议帧，
再转发给设备端。协议层用帧头、长度、序号、命令字、payload和checksum保证解析可靠性，
接收侧用RingBuffer处理半包、粘包和垃圾字节。这个项目重点是C/C++模块化、socket通信、
协议解析、命令分发、错误处理和可复现测试。
```

## 3-Minute Pitch

```text
项目分为三层：PC客户端、C++网关、设备端模拟器。PC客户端负责输入命令，网关负责命令解析、
协议封装和响应返回，设备端模拟器负责模拟设备状态和命令执行。协议格式是AA55帧头、LEN长度、
SEQ序号、CMD命令字、PAYLOAD和CHECKSUM。因为TCP和串口都是字节流，一次recv不一定是一帧，
所以我用RingBuffer缓存字节，再由解析器按帧头和长度提取完整帧。错误路径包括非法命令、
非法payload、checksum失败和设备超时。后续如果接真实设备，只需要把设备端通信后端从TCP模拟器
替换成串口termios，协议层和网关命令分发逻辑不用大改。
```
