SOF1 = 0xAA
SOF2 = 0x55


def checksum(data, begin, end):
    value = 0
    for b in data[begin:end]:
        value ^= b
    return value


def encode(seq, cmd, payload=b""):
    payload = bytes(payload)
    length = 1 + len(payload)
    out = bytearray([SOF1, SOF2, length, seq, cmd])
    out.extend(payload)
    out.append(checksum(out, 2, len(out)))
    return bytes(out)


def try_decode(buffer):
    while len(buffer) >= 2 and not (buffer[0] == SOF1 and buffer[1] == SOF2):
        buffer.pop(0)
    if len(buffer) < 6:
        return None

    length = buffer[2]
    total = 2 + 1 + 1 + length + 1
    if len(buffer) < total:
        return None

    raw = bytes(buffer[:total])
    if checksum(raw, 2, total - 1) != raw[-1]:
        buffer.pop(0)
        return None

    del buffer[:total]
    return {
        "seq": raw[3],
        "cmd": raw[4],
        "payload": raw[5:-1],
    }


def main():
    stream = bytearray()
    stream.extend(b"noise")
    stream.extend(encode(1, 0x01))
    stream.extend(encode(2, 0x04, b"50"))

    first = try_decode(stream)
    second = try_decode(stream)

    assert first == {"seq": 1, "cmd": 0x01, "payload": b""}
    assert second == {"seq": 2, "cmd": 0x04, "payload": b"50"}
    assert stream == bytearray()

    broken = bytearray(encode(3, 0x03, b"on"))
    broken[-1] ^= 0x10
    assert try_decode(broken) is None

    version = bytearray(encode(6, 0x06))
    assert try_decode(version) == {"seq": 6, "cmd": 0x06, "payload": b""}

    pwm = bytearray(encode(7, 0x04, bytes([50])))
    assert try_decode(pwm) == {"seq": 7, "cmd": 0x04, "payload": bytes([50])}

    print("protocol self-test passed")


if __name__ == "__main__":
    main()
