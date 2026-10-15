# ESP32-S3 Nano C2 Programmer

An experimental Silicon Labs C2 interface for the Waveshare ESP32-S3 Nano, targeting C8051F320/F321 devices. Includes an identification-only sketch, a programmer sketch, and a Python serial client.

**Status: hardware reading demonstrated on a Corsair H80i GT, as reported by the project owner.** The project owner confirms successful firmware extraction with this C2 programmer. Writing and erasing have not been confirmed. The supplied host-side tests also pass. The attached firmware dump is retained outside the public repository package; see the [hardware result](docs/hardware-result.md).

## Documentation

| Guide | Contents |
| --- | --- |
| [Wiring](docs/wiring.md) | GPIO mapping, power, and signal checks |
| [Setup and usage](docs/usage.md) | Arduino sketches and Python commands |
| [Serial protocol](docs/protocol.md) | Commands, replies, and error stages |
| [Hardware result](docs/hardware-result.md) | Reported H80i GT readout and supplied dump metadata |
| [Validation and limitations](docs/validation.md) | Offline tests and unresolved hardware questions |
| [File organization](docs/file-organization.md) | Included files and excluded attachments |
| [Optional analysis tools](tools/ghidra/README.md) | Ghidra export and SFR analysis scripts |
| [Security policy](SECURITY.md) | Reporting and handling dumps |
| [License status](LICENSE.md) | No license grant selected yet |

## Project layout

| Path | Purpose |
| --- | --- |
| `firmware/ESP32-C2-Reader/` | Standalone identification-only sketch, 115200 baud |
| `firmware/ESP32-C2-Programmer/` | C2 core, target table, and programmer sketch, 460800 baud |
| `cli/c2prog.py` | Python client for the programmer sketch |
| `tests/` | Offline Python tests and C++ GPIO simulation |
| `tests/stubs/Arduino.h` | Test-only Arduino substitute; never install it as an Arduino library |
| `tools/ghidra/` | Optional firmware-analysis utilities |

Each Arduino sketch is in a separate directory matching its main `.ino` filename. Upload one sketch at a time. The Python client talks to the programmer sketch; it does not support the reader's interactive protocol.

## Wiring at a glance

| ESP32-S3 Nano | Target signal |
| --- | --- |
| GND | Target GND / DBGGND |
| GPIO6 (D3 in the supplied source) | C2CK / DBGC2K |
| GPIO5 (D2 in the supplied source) | C2D / DBGC2D |
| Power pins | Leave disconnected; power the target separately |

Verify the actual board pin labels and target nets before connecting. See the [wiring guide](docs/wiring.md).

## Start with identification

Install and upload a sketch using the [setup guide](docs/usage.md). For the programmer sketch:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python cli/c2prog.py ports
python cli/c2prog.py --port /dev/cu.usbmodemXXXX info
```

The port above is a placeholder. Identification performs a reset sequence. Flash access can halt the target controller; test on an isolated board that is not responsible for active cooling or another critical load.

## Implemented capabilities

- Reader: passive digital samples and attempted Device/Revision ID reads.
- Programmer: identification, page-readability sampling, block read/write, page erase, and device erase.
- CLI: binary and Intel HEX input, write dry run, backup, verification, and explicit destructive-operation confirmation.
- Firmware write and erase commands require `ARM`; CLI write and erase require `--yes` to execute.

These are implemented code paths, not claims of working hardware support. Page erasure can affect the lock byte even though direct writes to that byte are rejected. See [limitations](docs/validation.md) before using flash commands.

## Contributing

Include the board model, source revision, target marking, measured signal levels, command, and sanitized output when reporting a problem. Distinguish real hardware observations from simulated test output. Keep dumps and credentials out of public issues.

The original source comments and terminal messages are preserved, including Swedish text. The repository guides are in English. Packaging does not change C2 timing, flash sequences, target constants, or client operation.
