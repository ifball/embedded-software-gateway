# Test Plan

Record test results in README or `test_log.md` before using the project in a
resume.

## Functional Tests

| Case | Command | Expected Result |
| --- | --- | --- |
| Read status | `status` | ACK with LED, PWM and sensor |
| Read sensor | `sensor` | ACK with sensor value |
| Read version | `version` | ACK with version |
| Turn LED on | `led on` | ACK and `led=on` |
| Turn LED off | `led off` | ACK and `led=off` |
| Set PWM normal | `pwm 50` | ACK and `pwm=50` |
| Set PWM lower bound | `pwm 0` | ACK and `pwm=0` |
| Set PWM upper bound | `pwm 100` | ACK and `pwm=100` |

## Error Tests

| Case | Command | Expected Result |
| --- | --- | --- |
| Bad command | `abc` | Gateway reports bad command |
| Missing value | `led` | Gateway reports bad command |
| Invalid PWM | `pwm 101` | Gateway reports bad command |
| Invalid PWM text | `pwm hello` | Gateway reports bad command |
| Runtime stats | `stats` | Gateway reports total/success/failed/timeout/avg_ms |

## Stability Tests

Run commands repeatedly for 30 minutes:

```text
status
sensor
led on
pwm 20
led off
pwm 80
```

Record:

- total commands
- success count
- error count
- timeout count
- whether gateway/device crashed

## STM32 Hardware Tests

After migration:

- UART DMA + IDLE receives complete frames.
- SensorTask still runs when commands are frequent.
- PWM output changes on oscilloscope or LED brightness.
- Queue depth does not overflow during command burst.
- Task stack high water mark is recorded.
