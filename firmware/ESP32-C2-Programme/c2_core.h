// C2-protokollkärna för ESP32. Sekvenser enligt Silicon Labs AN127 och Linux c2port-drivrutinen.
#pragma once
#include <Arduino.h>

// ---- GPIO ----
#if defined(CONFIG_IDF_TARGET_ESP32)
  #define PIN_C2CK 25   // -> DBGC2K  (klassisk ESP32 DevKit)
  #define PIN_C2D  26   // -> DBGC2D
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define PIN_C2CK 6    // D3 på Waveshare ESP32-S3 Nano -> DBGC2K
  #define PIN_C2D  5    // D2 -> DBGC2D (GND-pinnen sitter bredvid D2)
#else
  #define PIN_C2CK 4    // S2 / C3
  #define PIN_C2D  5
#endif

enum C2Result : int8_t {
  C2_OK         = 0,
  C2_E_TIMEOUT  = -1,   // inget svar på C2-nivå
  C2_E_STATUS   = -2,   // flashgränssnittet svarade med annan status än 0x0D (se c2LastStatus)
  C2_E_NOLINK   = -3,   // inget svar på ID-läsning (0x00/0xFF)
  C2_E_ARG      = -4,
  C2_E_RESET    = -5    // C2CK läses låg efter att resetpulsen släppts
};

extern uint8_t c2LastStatus;

void     c2Release();                                   // båda linjerna högimpediva
bool     c2Reset();                                     // reset + nivåkontroll; målreset/timing ej bevisad
char     c2ProbePin(uint8_t pin);                       // passiv sampling: 'H', 'L' eller 'U' (instabil)
C2Result c2ReadReg(uint8_t reg, uint8_t &v);
C2Result c2Connect(uint8_t &devId, uint8_t &revId);     // reset + läs Device/Revision ID
C2Result c2FlashEnable();                               // aktiverar flashgränssnittet, stoppar kärnan
C2Result c2BlockRead(uint16_t addr, uint8_t *buf, uint8_t len);        // len 1..128
C2Result c2BlockWrite(uint16_t addr, const uint8_t *buf, uint8_t len); // len 1..128, målet måste vara raderat
C2Result c2PageErase(uint8_t page);
C2Result c2DeviceErase();                               // fullständig radering (rensar även låsbyten)
