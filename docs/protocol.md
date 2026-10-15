# Serial Protocol

[Documentation index](../README.md#documentation)

## Programmer interface

The programmer uses newline-delimited ASCII commands at 460800 baud. Replies start with `OK` or `ERR <CODE>`. Code tokens and numeric formats are unchanged from the supplied implementation; some explanatory messages are in Swedish.

| Command | Purpose |
| --- | --- |
| `PING` | Report protocol version and configured GPIOs |
| `INFO` | Attempt reset and read target identity |
| `STATUS` | Sample the first byte of each configured page and read the lock byte |
| `READ <addr> <len>` | Read 1–128 bytes within the configured flash range |
| `ARM` | Enable write/erase command processing |
| `DISARM` | Disable write/erase command processing |
| `WRITE <addr> <hex>` | Write up to 128 bytes; requires ARM |
| `ERASE PAGE <n>` | Erase a configured page; requires ARM |
| `ERASE ALL` | Erase the device; requires ARM |
| `RESET` | Attempt target reset and release lines |

Addresses and numeric arguments use C-style base detection, including `0x` hexadecimal. WRITE data is a contiguous, even-length hexadecimal string. The firmware does not provide the CLI's backup or typed-confirmation workflow: `ARM` is a software guard, not authentication.

The following banner is an example derived from the S3 source constants, not a hardware capture:

```text
OK c2prog 1 ck=6 d=5
```

## Diagnostic stages

| Code | Meaning in the supplied implementation |
| --- | --- |
| `CKLOW` | C2CK sampled low before reset; attempt rejected |
| `CKUNSTABLE` | Two pre-reset C2CK samples differed; attempt rejected |
| `RESET` | C2CK sampled low after release of reset pulse |
| `COMM` | Invalid ID or C2 WAIT timeout; wording identifies the stage |
| `STATUS` | Flash interface returned a status other than expected 0x0D |
| `UNKNOWN_TARGET` | Device ID has no configured flash entry |
| `NOTARMED` | Write or erase requested without ARM |
| `RANGE` | Address or page is outside the permitted configured range |
| `BADARG` / `BADCMD` | Invalid arguments or unknown command |

The Python client also generates errors such as `NODEVICE`, `TIMEOUT`, and `BADREPLY`. An ESP32 handshake failure and a C2 target communication failure are different stages.

The reader sketch has a separate interactive interface at 115200 baud: `pins`, `id`, and `help`. It is not compatible with this protocol.
