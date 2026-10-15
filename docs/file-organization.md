# File Organization

[Documentation index](../README.md#documentation)

## Included source

| Supplied attachment | Repository location |
| --- | --- |
| ESP32-C2-Programmer(4).ino | firmware/ESP32-C2-Programmer/ESP32-C2-Programmer.ino |
| c2_core(3).cpp and c2_core(3).h | firmware/ESP32-C2-Programmer/ |
| targets(3).h | firmware/ESP32-C2-Programmer/targets.h |
| ESP32-C2-Reader(3).ino | firmware/ESP32-C2-Reader/ESP32-C2-Reader.ino |
| c2prog(4).py | cli/c2prog.py |
| Arduino.h | tests/stubs/Arduino.h |
| test_client.py and test_firmware.cpp | tests/; relative source paths updated |
| analyze.py and DumpFirmware.java | tools/ghidra/ |

Production source and analysis scripts are copied without changing their contents. Only test import/include paths were adjusted for the new layout. Source comments and runtime messages retain their original language.

## Deliberately omitted attachments

| Attachment | Reason |
| --- | --- |
| README(6).md | Replaced with structured English guides based on current source |
| README(2).pdf | Older duplicate guide with a conflicting S3 GPIO mapping |
| changes.patch | Historical before/after patch; examined files already contain the displayed changes |
| firmware-copy.bin | H80i GT dump supplied with the owner-reported readout; third-party firmware kept out of the public code package |
| functions.tsv | Generated analysis of an external firmware image, not programmer source |

The raw firmware dump and generated analysis remain outside the upload package. The optional analysis scripts require a user-supplied Ghidra project or exported instruction listing; no image is bundled.

This archive is a new repository package. Original uploaded attachments are unchanged.
