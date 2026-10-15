# Validation and Limitations

[Documentation index](../README.md#documentation)

## Checks performed during packaging

On 2026-10-15, the reorganized files passed 13 Python unit tests and both C++ host-test variants: programmer and reader. These tests run without hardware or serial ports.

The C++ harness checks selected frame construction, bit order, passive pre-reset gates, reset-level failure handling, communication timeout, numeric boundaries, and an ID-only path. The Python suite checks handshake and reply parsing, malformed responses, stale banners, and error propagation.

These tests do not validate electrical timing, real flash commands, target compatibility, or successful reset/recovery. The test Arduino header is a simulation, not an ESP32 implementation.

## Reproduce offline tests

From the repository root, use Python 3 and a C++17 compiler:

```sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
mkdir -p build
c++ -std=c++17 -DCONFIG_IDF_TARGET_ESP32S3 -Itests/stubs tests/test_firmware.cpp -o build/test-programmer
./build/test-programmer
c++ -std=c++17 -DCONFIG_IDF_TARGET_ESP32S3 -DTEST_READER -Itests/stubs tests/test_firmware.cpp -o build/test-reader
./build/test-reader
```

Do not define `NDEBUG` for these assertion-based tests. Arduino CLI is not installed in the packaging environment, so no build against the actual Arduino ESP32 core was performed.

## Configured target values

The following values are source configuration, not newly verified device specifications:

| Field | Current source value |
| --- | --- |
| Target label | C8051F320/F321 |
| Device ID | 0x09 |
| Flash size | 0x3E00 bytes |
| Page size | 512 bytes |
| Lock address | 0x3DFF |
| Flash-enable bytes | 0x02, 0x04, 0x01 |

The supplied README explicitly flags the flash geometry, page-erase sequence, block-write final status, and flash-enable sequence for confirmation. This packaging preserves those values and does not resolve them from external specifications.

## Known limits

- The project owner reports successful firmware extraction from a Corsair H80i GT using this programmer. The supplied older README predates that reported result.
- No complete acquisition transcript ties the successful readout to an exact version of the attached source. Flash writing and erasing remain unconfirmed. See the [hardware result](hardware-result.md).
- STATUS reads one byte per page. It is a sampling heuristic, not a complete readback test or reliable diagnosis of protection state.
- A flash status error can have causes other than read protection. The CLI's protection messages are heuristic.
- Direct WRITE rejects the lock address, but page erase can erase the page containing that byte. ERASE ALL also destroys it. ARM is required but does not prevent these operations.
- Erase can proceed without a backup when STATUS reports unreadable flash. A successfully created backup is not a guarantee of recoverability.
- The 30-second firmware idle cleanup is conditional on `flashUp`. It does not cover every linked or armed state.
- CLI cleanup attempts DISARM and RESET but suppresses cleanup exceptions. Target recovery must be verified independently.
- GPIO sampling and host simulations cannot prove pulse timing, signal integrity, or successful target reset.
- The Python HEX parser is a development implementation. Extended record handling and malformed-image cases are not covered by the supplied test suite.

Use a recoverable test target and verify actual waveforms before enabling destructive operations.
