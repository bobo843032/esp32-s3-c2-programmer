# Wiring

[Documentation index](../README.md#documentation)

## ESP32-S3 Nano mapping

The S3 branches in both supplied sketches select these GPIO numbers:

| Programmer pin | Target pad label in the supplied notes | Signal |
| --- | --- | --- |
| GND | DBGGND | Common ground |
| GPIO6 | DBGC2K | C2 clock / reset net |
| GPIO5 | DBGC2D | Bidirectional C2 data |
| No connection | DBGVDD / target power | Target uses its own supply |
| No additional connection by default | DBGRST | Verify whether this shares the C2CK net |

The source labels GPIO6 as D3 and GPIO5 as D2 on the Waveshare board. Use GPIO numbers and check the actual board pinout before wiring. Target pad labels are board-specific, not a universal connector standard.

The classic ESP32 code branch uses GPIO25/GPIO26. The fallback branch uses GPIO4/GPIO5. These alternatives are present in the source but are not validated board profiles.

## Checks before connecting

1. With power removed, establish continuity from the proposed pads to the intended MCU pins. Confirm the relationship between reset and C2CK.
2. Power the target from its own appropriate supply. Measure target logic and signal voltages against its ground; use compatible 3.3 V logic and never connect a 5 V signal directly to the ESP32.
3. Connect common ground, then C2CK and C2D. Do not tie the two boards' power rails together.
4. Keep wiring short. Begin with the reader's `pins` command, then attempt identification only after checking the measurements.

Digital sampling reports two logic samples. It cannot measure voltage or establish whether a line is floating. A high C2CK reading also does not prove correct wiring or a successful target reset.

## Target operation

The `id` and `INFO` paths attempt a reset. Flash operations can halt the controller, and reset recovery has not been verified on hardware. Disconnect controlled loads or provide independent cooling before experimenting with a cooling-controller board.
