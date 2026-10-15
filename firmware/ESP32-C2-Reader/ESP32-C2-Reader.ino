/*
  ESP32-C2-Reader  -  READ-ONLY C2-identifiering av Silicon Labs C8051F321

  Läser Device ID (reg 0x00, förväntat 0x09) och Revision ID (reg 0x01).
  Inget skriv-, flash-, radera- eller upplåsningsstöd finns i denna version.

  Kopplingar (se README i chatten):
    ESP32 GND       -> DBGGND
    ESP32 PIN_C2CK  -> DBGC2K  (samma nät som /RST på chippet, verifiera!)
    ESP32 PIN_C2D   -> DBGC2D
    DBGVDD          -> EJ ansluten (kortet matas av sitt eget nätagg)

  Seriekommandon (115200 baud, radslut LF):
    pins  - släpper båda linjerna och visar digitala nivåer (H / L / instabil)
    id    - resetar C2-gränssnittet och läser Device ID + Revision ID
    help  - visar kommandon

  OBS: 'id' ger en C2-reset, så kylarens MCU startar om.
  Protokollsekvenserna följer Silicon Labs AN127 / Linux c2port-drivrutinen
  (samma som c2tool). Ej kopierad kod. Ej testad mot hårdvara.
*/

#include <Arduino.h>

// ---- GPIO (ändra vid behov efter vilken ESP32-modell du har) ----
#if defined(CONFIG_IDF_TARGET_ESP32)
  #define PIN_C2CK 25   // klassisk ESP32 (DevKit)
  #define PIN_C2D  26
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define PIN_C2CK 6    // D3 på Waveshare ESP32-S3 Nano -> DBGC2K
  #define PIN_C2D  5    // D2 -> DBGC2D
#else
  #define PIN_C2CK 4    // S2 / C3
  #define PIN_C2D  5
#endif

// ---- Timing och tolerans ----
#define HALF_US          1      // halv klockperiod. C2CK låg >20 us = reset, håll låg-tiden kort!
#define WAIT_MAX         2000   // max antal WAIT-klockor innan timeout
#define EXTRA_TURNAROUND 0      // sätt 1 om ID läses förskjutet (felsökning)
#define EXPECTED_DEVID   0x09   // C8051F320/F321

// ---- C2-register (adresseras via Address Register) ----
#define C2_REG_DEVICEID  0x00
#define C2_REG_REVID     0x01

// ---------------------------------------------------------------
// Låg nivå
// ---------------------------------------------------------------
static void c2Release() {
  pinMode(PIN_C2CK, INPUT);
  pinMode(PIN_C2D, INPUT);
}

static inline void strobe() {
  noInterrupts();                       // håll låg-tiden kort och deterministisk
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

static inline uint8_t readBit() {       // läses efter stigande flank
  return digitalRead(PIN_C2D) ? 1 : 0;
}

static void c2dOutput() {
  digitalWrite(PIN_C2D, HIGH);
  pinMode(PIN_C2D, OUTPUT);
}

static bool c2Reset() {
  digitalWrite(PIN_C2CK, HIGH);
  pinMode(PIN_C2CK, OUTPUT);
  delayMicroseconds(10);
  digitalWrite(PIN_C2CK, LOW);
  delayMicroseconds(100);               // >=20 us, <5 ms
  digitalWrite(PIN_C2CK, HIGH);
  delay(2);
  pinMode(PIN_C2CK, INPUT);
  delayMicroseconds(2);
  if (!digitalRead(PIN_C2CK)) return false;
  pinMode(PIN_C2CK, OUTPUT);
  return true;                         // nivåkontroll; bevisar inte målreset/timing
}

// ---------------------------------------------------------------
// C2-transaktioner (endast adress-skrivning och data-läsning)
// ---------------------------------------------------------------
static void c2WriteAR(uint8_t addr) {
  pinMode(PIN_C2D, INPUT);
  strobe();                             // START: C2D-driver avstängd (AN127)
  c2dOutput();
  sendBit(1); sendBit(1);               // INS = 11b (Address Write)
  for (uint8_t i = 0; i < 8; i++) sendBit((addr >> i) & 1);  // LSB först
  pinMode(PIN_C2D, INPUT);
  strobe();                             // STOP
}

// true = OK, false = timeout i WAIT-fältet
static bool c2ReadDR(uint8_t &out) {
  pinMode(PIN_C2D, INPUT);
  strobe();                             // START: C2D-driver avstängd (AN127)
  c2dOutput();
  sendBit(0); sendBit(0);               // INS = 00b (Data Read)
  sendBit(0); sendBit(0);               // LENGTH = 00b (1 byte)
  pinMode(PIN_C2D, INPUT);
  if (EXTRA_TURNAROUND) strobe();

  bool ready = false;                   // WAIT: enheten håller 0 tills klar, sedan 1
  for (uint16_t n = 0; n < WAIT_MAX; n++) {
    strobe();
    if (readBit()) { ready = true; break; }
  }
  if (!ready) return false;

  uint8_t v = 0;
  for (uint8_t i = 0; i < 8; i++) {     // DATA, LSB först
    strobe();
    if (readBit()) v |= (1 << i);
  }
  strobe();                             // STOP
  out = v;
  return true;
}

static bool c2ReadReg(uint8_t reg, uint8_t &out) {
  c2WriteAR(reg);
  return c2ReadDR(out);
}

// ---------------------------------------------------------------
// Diagnostik
// ---------------------------------------------------------------
// Passiv digital sampling; kan inte identifiera flytande pinne eller mäta spänning.
static char probePin(uint8_t pin) {
  pinMode(pin, INPUT);
  int a = digitalRead(pin);
  delayMicroseconds(300);
  int b = digitalRead(pin);
  if (a != b) return 'U';
  return a ? 'H' : 'L';
}

static const char *pinText(char c) {
  return c == 'H' ? "H (två höga samplingar; spänning/flytande ej avgjort)"
       : c == 'L' ? "L (två låga samplingar; orsaken ej avgjord)"
                  : "instabil (samplingarna skiljer sig)";
}

static void cmdPins() {
  c2Release();
  delay(2);
  Serial.printf("C2CK  (GPIO%d): %s\n", PIN_C2CK, pinText(probePin(PIN_C2CK)));
  Serial.printf("C2D   (GPIO%d): %s\n", PIN_C2D,  pinText(probePin(PIN_C2D)));
}

static void cmdId() {
  c2Release();
  delay(2);
  char ck = probePin(PIN_C2CK);
  if (ck != 'H') {
    Serial.println(ck == 'L'
      ? "ERR CKLOW C2CK läses låg före reset; avbryter utan C2-reset eller ID-läsning"
      : "ERR CKUNSTABLE C2CK ändrades under nivåkontrollen; avbryter utan C2-reset eller ID-läsning");
    return;
  }

  if (!c2Reset()) {
    Serial.println("ERR RESET C2CK läses låg när resetpulsen släppts; ingen ID-läsning utfördes");
    c2Release();
    return;
  }

  uint8_t dev = 0, rev = 0;
  if (!c2ReadReg(C2_REG_DEVICEID, dev)) {
    Serial.println("ERR COMM WAIT-timeout vid läsning av Device ID; målreset/timing ej verifierad");
    Serial.println("  Kolla: 1) gemensam GND  2) C2D-tråden  3) att DBGC2K verkligen går till chippets C2CK/RST  4) att kortet har ström.");
    c2Release();
    return;
  }
  if (!c2ReadReg(C2_REG_REVID, rev)) {
    Serial.printf("Device ID: 0x%02X\n", dev);
    Serial.println("ERR COMM WAIT-timeout vid läsning av Revision ID; målreset/timing ej verifierad");
    c2Release();
    return;
  }
  c2Release();                          // släpp linjerna, kortet startar normalt

  Serial.printf("Device ID  : 0x%02X\n", dev);
  Serial.printf("Revision ID: 0x%02X\n", rev);

  if (dev == EXPECTED_DEVID) {
    Serial.println("OK: C8051F320/F321 bekräftad.");
  } else if (dev == 0xFF) {
    Serial.println("ERR COMM Device ID 0xFF; möjligt fel på C2D eller kommunikationen");
  } else if (dev == 0x00) {
    Serial.println("ERR COMM Device ID 0x00; möjligt fel på C2D eller kommunikationen");
  } else {
    Serial.println("OVÄNTAT Device ID; kontrollera chipmodell och uppmätta C2-signaler före fler försök.");
  }
}

static void cmdHelp() {
  Serial.println("Kommandon: pins | id | help");
}

// ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(600);
  c2Release();                           // båda linjerna högimpediva tills du kör 'id'
  Serial.println();
  Serial.println("ESP32-C2-Reader (read-only)");
  Serial.printf("C2CK=GPIO%d  C2D=GPIO%d\n", PIN_C2CK, PIN_C2D);
  cmdHelp();
}

void loop() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  s.toLowerCase();
  if (s.length() == 0) return;

  if (s == "id")         cmdId();
  else if (s == "pins")  cmdPins();
  else                   cmdHelp();
}
