#include "c2_core.h"

uint8_t c2LastStatus = 0;

#define HALF_US 1        // halv klockperiod. C2CK låg >20 us = reset, håll låg-tiden kort!

#define REG_DEVICEID 0x00
#define REG_REVID    0x01
#define REG_FPCTL    0x02
#define REG_FPDAT    0xB4

#define CMD_DEVICE_ERASE 0x03
#define CMD_BLOCK_READ   0x06
#define CMD_BLOCK_WRITE  0x07
#define CMD_PAGE_ERASE   0x08
#define ST_OK            0x0D

#define CHK(x) do { C2Result _r = (x); if (_r != C2_OK) return _r; } while (0)

// ---------------- signalnivå ----------------
void c2Release() {
  pinMode(PIN_C2CK, INPUT);
  pinMode(PIN_C2D, INPUT);
}

static inline void strobe() {
  noInterrupts();
  digitalWrite(PIN_C2CK, LOW);
  delayMicroseconds(HALF_US);
  digitalWrite(PIN_C2CK, HIGH);
  interrupts();
  delayMicroseconds(HALF_US);
}

static inline void sendBit(uint8_t b) {
  digitalWrite(PIN_C2D, b ? HIGH : LOW);
  strobe();
}

static inline void c2dOutput() {
  digitalWrite(PIN_C2D, HIGH);
  pinMode(PIN_C2D, OUTPUT);
}

bool c2Reset() {
  digitalWrite(PIN_C2CK, HIGH);
  pinMode(PIN_C2CK, OUTPUT);
  delayMicroseconds(10);
  noInterrupts();
  digitalWrite(PIN_C2CK, LOW);
  delayMicroseconds(50);                // >=20 us, <5 ms
  digitalWrite(PIN_C2CK, HIGH);
  interrupts();
  delay(2);
  pinMode(PIN_C2D, INPUT);
  pinMode(PIN_C2CK, INPUT);              // läs den släppta linjen, inte utgångslatchen
  delayMicroseconds(2);
  if (!digitalRead(PIN_C2CK)) return false;
  pinMode(PIN_C2CK, OUTPUT);
  return true;                         // nivåkontroll; bevisar inte målreset/timing
}

char c2ProbePin(uint8_t pin) {
  pinMode(pin, INPUT);                  // säkerhetskontroll utan att belasta resetnätet
  int a = digitalRead(pin);
  delayMicroseconds(300);
  int b = digitalRead(pin);
  if (a != b) return 'U';
  return a ? 'H' : 'L';
}

// ---------------- C2-transaktioner ----------------
static void writeAR(uint8_t addr) {
  pinMode(PIN_C2D, INPUT);
  strobe();                             // START: C2D-driver avstängd (AN127)
  c2dOutput();
  sendBit(1); sendBit(1);               // INS 11b
  for (uint8_t i = 0; i < 8; i++) sendBit((addr >> i) & 1);
  pinMode(PIN_C2D, INPUT);
  strobe();                             // STOP
}

static uint8_t readAR() {
  pinMode(PIN_C2D, INPUT);
  strobe();                             // START: C2D-driver avstängd (AN127)
  c2dOutput();
  sendBit(0); sendBit(1);               // INS 10b
  pinMode(PIN_C2D, INPUT);
  uint8_t v = 0;
  for (uint8_t i = 0; i < 8; i++) {
    v >>= 1;
    strobe();
    if (digitalRead(PIN_C2D)) v |= 0x80;
  }
  strobe();                             // STOP
  return v;
}

static C2Result writeDR(uint8_t data) {
  pinMode(PIN_C2D, INPUT);
  strobe();                             // START: C2D-driver avstängd (AN127)
  c2dOutput();
  sendBit(1); sendBit(0);               // INS 01b
  sendBit(0); sendBit(0);               // LENGTH 1 byte
  for (uint8_t i = 0; i < 8; i++) sendBit((data >> i) & 1);
  pinMode(PIN_C2D, INPUT);
  for (uint16_t n = 0;; n++) {          // WAIT
    if (n >= 5000) return C2_E_TIMEOUT;
    strobe();
    if (digitalRead(PIN_C2D)) break;
  }
  strobe();                             // STOP
  return C2_OK;
}

static C2Result readDR(uint8_t &out) {
  pinMode(PIN_C2D, INPUT);
  strobe();                             // START: C2D-driver avstängd (AN127)
  c2dOutput();
  sendBit(0); sendBit(0);               // INS 00b
  sendBit(0); sendBit(0);               // LENGTH 1 byte
  pinMode(PIN_C2D, INPUT);
  for (uint16_t n = 0;; n++) {          // WAIT
    if (n >= 5000) return C2_E_TIMEOUT;
    strobe();
    if (digitalRead(PIN_C2D)) break;
  }
  uint8_t v = 0;
  for (uint8_t i = 0; i < 8; i++) {
    v >>= 1;
    strobe();
    if (digitalRead(PIN_C2D)) v |= 0x80;
  }
  strobe();                             // STOP
  out = v;
  return C2_OK;
}

static C2Result pollInBusy(uint32_t ms) {       // väntar tills InBusy (bit1) släpper
  uint32_t t0 = millis();
  do { if (!(readAR() & 0x02)) return C2_OK; } while (millis() - t0 < ms);
  return C2_E_TIMEOUT;
}

static C2Result pollOutReady(uint32_t ms) {     // väntar på OutReady (bit0)
  uint32_t t0 = millis();
  do { if (readAR() & 0x01) return C2_OK; } while (millis() - t0 < ms);
  return C2_E_TIMEOUT;
}

static C2Result readStatus(uint32_t ms) {
  CHK(pollOutReady(ms));
  uint8_t st;
  CHK(readDR(st));
  c2LastStatus = st;
  return st == ST_OK ? C2_OK : C2_E_STATUS;
}

// ---------------- API ----------------
C2Result c2ReadReg(uint8_t reg, uint8_t &v) {
  writeAR(reg);
  return readDR(v);
}

C2Result c2Connect(uint8_t &devId, uint8_t &revId) {
  c2Release();
  delay(1);
  if (!c2Reset()) return C2_E_RESET;
  CHK(c2ReadReg(REG_DEVICEID, devId));
  CHK(c2ReadReg(REG_REVID, revId));
  if (devId == 0x00 || devId == 0xFF) return C2_E_NOLINK;
  return C2_OK;
}

C2Result c2FlashEnable() {
  writeAR(REG_FPCTL);
  CHK(writeDR(0x02));
  CHK(writeDR(0x04));
  CHK(writeDR(0x01));
  delay(25);                            // minst 20 ms
  return C2_OK;
}

C2Result c2BlockRead(uint16_t addr, uint8_t *buf, uint8_t len) {
  if (len == 0 || len > 128) return C2_E_ARG;
  writeAR(REG_FPDAT);
  CHK(writeDR(CMD_BLOCK_READ));
  CHK(pollInBusy(50));
  CHK(readStatus(100));
  CHK(writeDR(addr >> 8));   CHK(pollInBusy(50));
  CHK(writeDR(addr & 0xFF)); CHK(pollInBusy(50));
  CHK(writeDR(len));         CHK(pollInBusy(50));
  CHK(readStatus(100));      // här avvisas läsning av skyddade sidor
  for (uint8_t i = 0; i < len; i++) {
    CHK(pollOutReady(50));
    CHK(readDR(buf[i]));
  }
  return C2_OK;
}

C2Result c2BlockWrite(uint16_t addr, const uint8_t *buf, uint8_t len) {
  if (len == 0 || len > 128) return C2_E_ARG;
  writeAR(REG_FPDAT);
  CHK(writeDR(CMD_BLOCK_WRITE));
  CHK(pollInBusy(50));
  CHK(readStatus(100));
  CHK(writeDR(addr >> 8));   CHK(pollInBusy(50));
  CHK(writeDR(addr & 0xFF)); CHK(pollInBusy(50));
  CHK(writeDR(len));         CHK(pollInBusy(50));
  CHK(readStatus(100));
  for (uint8_t i = 0; i < len; i++) {
    CHK(writeDR(buf[i]));
    CHK(pollInBusy(50));
  }
  CHK(readStatus(100));      // slutstatus efter sista byten
  return C2_OK;
}

C2Result c2PageErase(uint8_t page) {
  writeAR(REG_FPDAT);
  CHK(writeDR(CMD_PAGE_ERASE));
  CHK(pollInBusy(50));
  CHK(readStatus(100));
  CHK(writeDR(page));        CHK(pollInBusy(50));
  CHK(readStatus(100));
  CHK(writeDR(0x00));        CHK(pollInBusy(50));   // bekräfta radering
  CHK(readStatus(1000));
  return C2_OK;
}

C2Result c2DeviceErase() {
  writeAR(REG_FPDAT);
  CHK(writeDR(CMD_DEVICE_ERASE));
  CHK(pollInBusy(50));
  CHK(readStatus(100));
  CHK(writeDR(0xDE)); CHK(pollInBusy(50));          // armeringssekvens DE AD A5
  CHK(writeDR(0xAD)); CHK(pollInBusy(50));
  CHK(writeDR(0xA5)); CHK(pollInBusy(50));
  CHK(readStatus(5000));
  return C2_OK;
}
