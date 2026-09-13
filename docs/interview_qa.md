# Interview Q&A

## Why not send plain text commands directly to STM32?

Plain text is easy for debugging, but a framed binary protocol is easier to
parse reliably on a byte stream. Header, length and checksum help handle
sticky packets, partial packets and corrupted data.

## Why use RingBuffer?

UART and TCP both produce byte streams. One receive event may contain half a
frame, exactly one frame, or multiple frames. RingBuffer keeps bytes until the
parser has enough data to decode a complete frame.

## How do you handle sticky packets?

The parser searches for `AA 55`, reads `LEN`, then waits until the full frame
arrives. If two frames arrive together, the parser decodes one and continues
decoding the next from the same buffer.

## How do you handle corrupted frames?

The checksum is XOR of `LEN`, `SEQ`, `CMD` and `PAYLOAD`. If checksum fails,
the parser drops one byte and searches again for the next valid header.

## Why split FreeRTOS tasks?

Parsing, command execution, sensor collection and UART sending have different
timing and blocking behavior. Splitting them makes the system easier to debug:
each task has a clear input, output and blocking point.

## Why use Queue instead of shared variables?

Queue is better for passing ownership of messages between tasks. Shared
variables need mutex protection and are easier to misuse when command bursts
arrive.

## Why use Semaphore from ISR?

The UART IDLE interrupt should do minimum work. It gives a semaphore to wake a
task, and the task performs parsing outside interrupt context.

## Why use Mutex?

Mutex protects shared resources such as UART TX or device state. Unlike a
binary semaphore, a mutex expresses ownership and can support priority
inheritance in FreeRTOS.

## What would you optimize next?

- Add reconnect state machine in gateway.
- Add more binary payload structs.
- Add CRC16 instead of XOR checksum.
- Add more unit tests for protocol parser.
- Add 30-minute or 2-hour stability test logs.
- Add real serial `termios` backend for Linux.

## What have you already tested?

The project includes protocol tests for:

- encode/decode round trip
- sticky packets
- partial packets
- garbage bytes before frame header
- checksum failure recovery

The gateway also exposes runtime statistics with `stats`.
