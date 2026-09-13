# Test Log

Date:
Machine:
Compiler:
Commit:

## Build

Command:

```text
cmake -S . -B build
cmake --build build --config Release
```

Result:

```text
TODO: paste build result
```

## Protocol Tests

Command:

```text
.\build\Release\protocol_tests.exe
python .\tools_protocol_selftest.py
```

Result:

```text
TODO: paste test result
```

## Functional Run

Commands:

```text
status
sensor
version
led on
pwm 50
stats
led off
pwm 100
reset_error
quit
```

Result:

```text
TODO: paste client output
```

## Error Cases

Commands:

```text
abc
led
pwm hello
pwm 101
```

Result:

```text
TODO: paste client output
```

## Summary

```text
total commands:
success:
failed:
timeout:
known issues:
```
