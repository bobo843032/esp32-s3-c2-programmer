# Setup and Usage

[Documentation index](../README.md#documentation)

## Arduino setup

Use Arduino IDE or Arduino CLI with the Espressif ESP32 board package. The supplied guide specifies this board-manager URL:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Select ESP32-S3 with USB CDC enabled at boot. The FQBN carried over from the supplied guide is `esp32:esp32:esp32s3:CDCOnBoot=cdc`. It has not been compiled against an installed Arduino core during this packaging task; verify the available board options in your installation.

Run from the repository root, substituting your actual serial port:

```sh
arduino-cli board list
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc firmware/ESP32-C2-Reader
arduino-cli upload --port /dev/cu.usbmodemXXXX --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc firmware/ESP32-C2-Reader
```

To use the full programmer, replace both occurrences of `firmware/ESP32-C2-Reader` with `firmware/ESP32-C2-Programmer`. Do not combine the sketches into one Arduino folder.

## Identification-only reader

Open a serial terminal at 115200 baud with LF line endings. Close it before another program opens the same port.

| Command | Behavior |
| --- | --- |
| `pins` | Release lines and sample their digital levels |
| `id` | Attempt reset, Device ID, and Revision ID reads |
| `help` | Show available commands |

The reader contains no flash write or erase operations. Identification still changes target state through reset.

## Python client

The client requires the full programmer sketch, using 460800 baud by default. Python 3.7 or newer is required by the supplied code's standard-library APIs.

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python cli/c2prog.py ports
python cli/c2prog.py --port /dev/cu.usbmodemXXXX info
```

Place global options such as `--port` and `--baud` before the subcommand. Alternatively set `C2PROG_PORT`. Automatic discovery uses macOS device-name patterns; use an explicit port on other systems.

## Flash operations

The following are command examples, not recorded hardware results. Use only after resolving the target and timing limitations in the [validation guide](validation.md).

```sh
python cli/c2prog.py --port /dev/cu.usbmodemXXXX status
python cli/c2prog.py --port /dev/cu.usbmodemXXXX read dump.bin
python cli/c2prog.py --port /dev/cu.usbmodemXXXX verify image.hex
python cli/c2prog.py --port /dev/cu.usbmodemXXXX write image.hex
```

`write` without `--yes` does not program or erase flash, but it still communicates with the target and checks status. A write with `--yes` attempts a backup, merges affected pages, erases them, writes data, and reads it back. Binary images use address zero unless `--base` is supplied. Do not truncate an existing firmware dump merely to bypass the lock-address check.

```sh
python cli/c2prog.py --port /dev/cu.usbmodemXXXX write image.hex --yes
python cli/c2prog.py --port /dev/cu.usbmodemXXXX erase --page 3 --yes
python cli/c2prog.py --port /dev/cu.usbmodemXXXX erase --all --yes
```

These last three examples are destructive. Full erase additionally requests the literal confirmation `ERASE`. Page erase has no equivalent typed confirmation. If the status check reports unreadable flash, the current erase path can continue without a backup; do not describe automatic backup as unconditional.

Backups are written under `backups/` relative to the current working directory. `read` prints a full SHA-256 digest; the backup message prints a shortened digest. Neither validates the hardware's correctness by itself.
