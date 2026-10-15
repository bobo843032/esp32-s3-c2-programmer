# Hardware Result: Corsair H80i GT

[Documentation index](../README.md#documentation)

## Reported successful readout

The project owner confirms that this ESP32-S3 Nano C2 programmer was tested on a Corsair H80i GT controller and successfully read its firmware. This updates the older supplied README, which described the project as not yet hardware-tested.

| Operation | Status |
| --- | --- |
| Firmware readout on H80i GT | Successful, reported by project owner |
| Flash writing | Not confirmed |
| Page or device erase | Not confirmed |
| Other target boards | Not established by this result |

No serial transcript or exact tested source revision was provided with the result. The report establishes a successful project-level hardware readout; it does not validate every operation in the packaged source.

## Supplied dump metadata

The following metadata was calculated from the supplied file during packaging:

| Field | Value |
| --- | --- |
| Supplied filename | firmware-copy.bin |
| Size | 16384 bytes (0x4000) |
| SHA-256 | `eab09a20ea0e7e2b6d5f54f692298bf1d58288ff1fdf427ee29345ec49494ed1` |

The dump is not included in the public repository package. Its hash is recorded for identification, not as a claim of permission to redistribute the firmware.

The attached target table configures a 0x3E00-byte flash range, while the supplied dump is 0x4000 bytes. The evidence does not explain the difference. Preserve the original dump; do not infer a flash layout or truncate it to make it match the current source constants.
