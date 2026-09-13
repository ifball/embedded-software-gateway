# Gateway State Machine

The gateway is not just a socket forwarding demo. It follows a simple device
communication state machine.

```text
START
  |
  v
CONNECT_DEVICE
  | success
  v
WAIT_PC_CLIENT
  | client connected
  v
IDLE
  | command received
  v
PARSE_COMMAND
  | valid                         | invalid
  v                               v
ENCODE_FRAME                  REPORT_BAD_COMMAND
  |
  v
SEND_TO_DEVICE
  | success                       | send failed
  v                               v
WAIT_DEVICE_RESPONSE          DEVICE_LINK_ERROR
  | response                      | timeout
  v                               v
DECODE_RESPONSE               DEVICE_TIMEOUT
  |
  v
UPDATE_STATS
  |
  v
SEND_TO_PC
  |
  v
IDLE
```

## State Responsibilities

| State | Responsibility |
| --- | --- |
| CONNECT_DEVICE | Connect to current TCP simulator backend |
| WAIT_PC_CLIENT | Wait for PC command client |
| IDLE | Keep connection alive and receive command line |
| PARSE_COMMAND | Convert text command to internal request |
| ENCODE_FRAME | Build `AA 55 LEN SEQ CMD PAYLOAD CHECKSUM` frame |
| SEND_TO_DEVICE | Send all bytes, not just one partial send |
| WAIT_DEVICE_RESPONSE | Apply receive timeout |
| DECODE_RESPONSE | Use RingBuffer and protocol parser |
| UPDATE_STATS | Count success, failure, timeout and latency |
| SEND_TO_PC | Return readable result |

## Why This Matters

This state machine is what makes the project an embedded software gateway
instead of a loose demo. It gives clear places for timeout, reconnect, logging,
statistics and backend replacement.

## Next Production Upgrade

Add a real reconnect loop:

```text
DEVICE_TIMEOUT or DEVICE_LINK_ERROR
  -> CLOSE_DEVICE
  -> CONNECT_DEVICE
  -> replay or reject current request
```
