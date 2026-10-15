// Måltabell. Lägg till fler Silicon Labs C2-kretsar här (värden från respektive datablad).
#pragma once
#include <stdint.h>

struct Target {
  uint8_t  devId;      // C2 DEVICEID
  const char *name;    // inga mellanslag (används i protokollet)
  uint16_t flashSize;  // läsbar/skrivbar kodflash i byte, inklusive låsbyten
  uint16_t pageSize;   // raderingssida
  uint16_t lockAddr;   // adress till låsbyten. Verktyget skriver aldrig hit eller över.
};

static const Target TARGETS[] = {
  // C8051F320/F321: 0x0000-0x3DFF kodflash, 0x3E00-0x3FFF reserverat, låsbyte på 0x3DFF, 512 B sidor.
  // Kontrollera mot databladets flash-avsnitt innan första skrivning.
  { 0x09, "C8051F320/F321", 0x3E00, 512, 0x3DFF },
};

static inline const Target *findTarget(uint8_t devId) {
  for (const Target &t : TARGETS) if (t.devId == devId) return &t;
  return nullptr;
}
