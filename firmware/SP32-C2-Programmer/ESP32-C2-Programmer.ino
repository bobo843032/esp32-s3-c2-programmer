/*
  ESP32-C2-Programmer - C2-programmerare för Silicon Labs C8051F32x

  Radbaserat protokoll över USB-serie (460800 baud), använd med cli/c2prog.py.
  Svar: "OK ..." eller "ERR <KOD> <text>".

    PING
    INFO                       reset + läs Device/Revision ID (kärnan fortsätter köra)
    STATUS                     testläser varje sida, visar läsbarhet och låsbyte
    READ <addr> <len>          len 1..128, svar: OK <hex>
    ARM / DISARM               skriv- och raderingskommandon kräver ARM
    ERASE PAGE <n>             (ARM)
    ERASE ALL                  (ARM) fullständig radering, rensar även lås
    WRITE <addr> <hex>         (ARM) max 128 byte, aldrig över/på låsbyten
    RESET                      reset av målet och släpp linjerna

  Standardläge är läsning. Inget i firmware försöker kringgå lässkydd.
*/

#include "c2_core.h"
#include "targets.h"
#include <errno.h>

#define IDLE_TIMEOUT_MS 30000

static const Target *tgt = nullptr;
static bool linkUp = false;      // C2-reset gjord och ID läst
static bool flashUp = false;     // flashgränssnitt aktivt (kärnan stoppad)
static bool armed = false;
static uint8_t devId = 0, revId = 0;
static uint32_t lastCmdMs = 0;

// ---------------- hjälpfunktioner ----------------
static bool endLink() {          // reset så att målet kör igen, släpp linjerna
  bool resetOk = true;
  if (linkUp || flashUp) resetOk = c2Reset();
  c2Release();
  linkUp = flashUp = armed = false;
  return resetOk;
}

static void failC2(C2Result r) {
  if (r == C2_E_STATUS)
    Serial.printf("ERR STATUS flashgränssnittet svarade 0x%02X (kan bero på lässkydd eller ogiltig adress)\n", c2LastStatus);
  else if (r == C2_E_NOLINK)
    Serial.println("ERR COMM ogiltigt C2 Device ID 0x00/0xFF efter resetsekvens; kontrollera C2D och signalerna");
  else if (r == C2_E_TIMEOUT)
    Serial.println("ERR COMM WAIT-timeout i C2-kommunikation; målreset och klocktiming är inte verifierade");
  else if (r == C2_E_RESET)
    Serial.println("ERR RESET C2CK läses låg när resetpulsen släppts; ingen ID-läsning utfördes");
  else
    Serial.println("ERR BADARG ogiltigt argument");
  endLink();
}

static bool connectLink(bool needFlash) {
  if (!linkUp) {
    c2Release();
    delay(2);
    char ck = c2ProbePin(PIN_C2CK);
    if (ck != 'H') {
      Serial.println(ck == 'L'
        ? "ERR CKLOW C2CK läses låg före reset; avbryter utan C2-reset eller ID-läsning"
        : "ERR CKUNSTABLE C2CK ändrades under nivåkontrollen; avbryter utan C2-reset eller ID-läsning");
      return false;
    }
    uint8_t d = 0, r = 0;
    C2Result res = c2Connect(d, r);
    if (res != C2_OK) { failC2(res); return false; }
    devId = d; revId = r;
    tgt = findTarget(d);
    linkUp = true;
    flashUp = false;
  }
  if (needFlash && !flashUp) {
    if (!tgt) {
      Serial.printf("ERR UNKNOWN_TARGET Device ID 0x%02X finns inte i måltabellen, flashåtkomst nekas\n", devId);
      return false;
    }
    C2Result res = c2FlashEnable();
    if (res != C2_OK) { failC2(res); return false; }
    flashUp = true;
  }
  return true;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int parseHex(const char *s, uint8_t *out, size_t maxBytes) {
  size_t n = strlen(s);
  if (n == 0 || (n & 1) || n / 2 > maxBytes) return -1;
  for (size_t i = 0; i < n / 2; i++) {
    int h = hexVal(s[2 * i]), l = hexVal(s[2 * i + 1]);
    if (h < 0 || l < 0) return -1;
    out[i] = (h << 4) | l;
  }
  return (int)(n / 2);
}

static bool parseNum(const char *s, uint32_t &v) {
  if (!s || *s == '-') return false;
  char *end;
  errno = 0;
  unsigned long parsed = strtoul(s, &end, 0);
  if (errno == ERANGE || end == s || *end != 0 || parsed > UINT32_MAX) return false;
  v = (uint32_t)parsed;
  return true;
}

static void badArg(const char *why) { Serial.printf("ERR BADARG %s\n", why); }

// ---------------- kommandon ----------------
static void cmdInfo() {
  if (linkUp && !endLink()) { failC2(C2_E_RESET); return; }
  if (!connectLink(false)) return;
  if (tgt)
    Serial.printf("OK dev=%02X rev=%02X name=%s flash=%u page=%u lock=%04X known=1\n",
                  devId, revId, tgt->name, tgt->flashSize, tgt->pageSize, tgt->lockAddr);
  else
    Serial.printf("OK dev=%02X rev=%02X name=unknown flash=0 page=0 lock=0000 known=0\n", devId, revId);
}

static void cmdStatus() {
  if (!connectLink(true)) return;
  uint8_t pages = tgt->flashSize / tgt->pageSize, readable = 0;
  uint32_t mask = 0;
  for (uint8_t p = 0; p < pages; p++) {
    uint8_t b;
    C2Result r = c2BlockRead((uint16_t)p * tgt->pageSize, &b, 1);
    if (r == C2_OK) { mask |= 1UL << p; readable++; }
    else if (r != C2_E_STATUS) { failC2(r); return; }
  }
  uint8_t lb = 0;
  C2Result r = c2BlockRead(tgt->lockAddr, &lb, 1);
  if (r != C2_OK && r != C2_E_STATUS) { failC2(r); return; }
  Serial.printf("OK pages=%u readable=%u mask=%08lX lock=", pages, readable, (unsigned long)mask);
  if (r == C2_OK) Serial.printf("%02X\n", lb); else Serial.println("NA");
}

static void cmdRead(char **t, int nt) {
  uint32_t addr, len;
  if (nt < 3 || !parseNum(t[1], addr) || !parseNum(t[2], len) || len < 1 || len > 128) { badArg("READ <addr> <1..128>"); return; }
  if (!connectLink(true)) return;
  if (addr >= tgt->flashSize || len > tgt->flashSize - addr) { Serial.println("ERR RANGE utanför flashen"); return; }
  uint8_t buf[128];
  C2Result r = c2BlockRead((uint16_t)addr, buf, (uint8_t)len);
  if (r != C2_OK) { failC2(r); return; }
  static const char *H = "0123456789ABCDEF";
  char out[2 * 128 + 1];
  for (uint32_t i = 0; i < len; i++) { out[2 * i] = H[buf[i] >> 4]; out[2 * i + 1] = H[buf[i] & 15]; }
  out[2 * len] = 0;
  Serial.print("OK "); Serial.println(out);
}

static void cmdWrite(char **t, int nt) {
  if (!armed) { Serial.println("ERR NOTARMED skrivning kräver ARM"); return; }
  uint32_t addr;
  uint8_t buf[128];
  int len;
  if (nt < 3 || !parseNum(t[1], addr) || (len = parseHex(t[2], buf, 128)) < 0) { badArg("WRITE <addr> <hex, max 128 byte>"); return; }
  if (!connectLink(true)) return;
  if (addr >= tgt->lockAddr || (uint32_t)len > tgt->lockAddr - addr) { Serial.println("ERR RANGE skrivning får inte nå låsbyten eller reserverad yta"); return; }
  C2Result r = c2BlockWrite((uint16_t)addr, buf, (uint8_t)len);
  if (r != C2_OK) { failC2(r); return; }
  Serial.println("OK");
}

static void cmdErase(char **t, int nt) {
  if (!armed) { Serial.println("ERR NOTARMED radering kräver ARM"); return; }
  if (nt < 2) { badArg("ERASE PAGE <n> | ERASE ALL"); return; }
  for (char *c = t[1]; *c; c++) *c = toupper(*c);
  if (!strcmp(t[1], "PAGE")) {
    uint32_t n;
    if (nt < 3 || !parseNum(t[2], n)) { badArg("ERASE PAGE <n>"); return; }
    if (!connectLink(true)) return;
    if (n >= (uint32_t)(tgt->flashSize / tgt->pageSize)) { Serial.println("ERR RANGE sidnummer utanför flashen"); return; }
    C2Result r = c2PageErase((uint8_t)n);
    if (r != C2_OK) { failC2(r); return; }
    Serial.println("OK");
  } else if (!strcmp(t[1], "ALL")) {
    if (!connectLink(true)) return;
    C2Result r = c2DeviceErase();
    if (r != C2_OK) { failC2(r); return; }
    endLink();                             // reset; nästa kommando ansluter på nytt
    Serial.println("OK erased-all");
  } else {
    badArg("ERASE PAGE <n> | ERASE ALL");
  }
}

static void handleLine(char *line) {
  lastCmdMs = millis();
  char *t[4];
  int nt = 0;
  char *sp;
  for (char *p = strtok_r(line, " \t", &sp); p && nt < 4; p = strtok_r(nullptr, " \t", &sp)) t[nt++] = p;
  if (nt == 0) return;
  for (char *c = t[0]; *c; c++) *c = toupper(*c);

  if (!strcmp(t[0], "PING"))         Serial.printf("OK c2prog 1 ck=%d d=%d\n", PIN_C2CK, PIN_C2D);
  else if (!strcmp(t[0], "INFO"))    cmdInfo();
  else if (!strcmp(t[0], "STATUS"))  cmdStatus();
  else if (!strcmp(t[0], "READ"))    cmdRead(t, nt);
  else if (!strcmp(t[0], "ARM"))     { armed = true;  Serial.println("OK armed"); }
  else if (!strcmp(t[0], "DISARM"))  { armed = false; Serial.println("OK disarmed"); }
  else if (!strcmp(t[0], "WRITE"))   cmdWrite(t, nt);
  else if (!strcmp(t[0], "ERASE"))   cmdErase(t, nt);
  else if (!strcmp(t[0], "RESET"))   { if (endLink()) Serial.println("OK"); else failC2(C2_E_RESET); }
  else                               Serial.println("ERR BADCMD okänt kommando");
}

// ---------------- Arduino ----------------
void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(460800);
  c2Release();                             // linjerna högimpediva tills ett kommando kräver dem
}

void loop() {
  static char line[600];
  static size_t n = 0;
  static bool overflow = false;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[n] = 0;
      if (overflow) Serial.println("ERR BADARG raden är för lång");
      else handleLine(line);
      n = 0; overflow = false;
    } else if (n < sizeof(line) - 1) {
      line[n++] = c;
    } else {
      overflow = true;
    }
  }

  if (flashUp && millis() - lastCmdMs > IDLE_TIMEOUT_MS) endLink();   // släpp kärnan om värden försvinner
}
