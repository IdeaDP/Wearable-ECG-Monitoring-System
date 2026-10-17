#include <Arduino.h>

// -------- Pin config --------
#define ECG_PIN PA7
#define SDN_PIN PA4
#define LED_PIN PB11   

// -------- Timing: 250 SPS --------
unsigned long lastTime = 0;
const unsigned long interval_us = 4000;
uint32_t sampleCount = 0;

// -------- Packetization --------
#define PACKET_SAMPLES 5
static int16_t packetBuf[PACKET_SAMPLES];
static uint8_t packetIdx = 0;
static uint8_t packetCounter = 0;   // rolling 0-255, used for gap detection

// -------- Binary framing --------
#define SYNC1 0xAA
#define SYNC2 0x55
#define FRAME_LEN (2 + 1 + PACKET_SAMPLES * 2 + 1)  // 14 bytes

// -------- DC removal --------
float baseline = 0;
const float alphaDC = 0.0063f;
bool baselineInit = false;

// -------- LED sequence state machine --------
enum LedState { LED_BLINK, LED_SOLID, LED_DONE };
LedState ledState = LED_BLINK;
unsigned long ledPhaseStart = 0;
unsigned long lastBlinkToggle = 0;
bool ledOn = false;
const unsigned long BLINK_PHASE_MS = 10000;   // 10 s blinking
const unsigned long SOLID_PHASE_MS = 10000;   // 10 s solid on
const unsigned long BLINK_INTERVAL_MS = 250;  // toggle every 250 ms while blinking

static void updateLed() {
  unsigned long now = millis();

  switch (ledState) {
    case LED_BLINK:
      if (now - lastBlinkToggle >= BLINK_INTERVAL_MS) {
        lastBlinkToggle = now;
        ledOn = !ledOn;
        digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
      }
      if (now - ledPhaseStart >= BLINK_PHASE_MS) {
        ledState = LED_SOLID;
        ledPhaseStart = now;
        digitalWrite(LED_PIN, HIGH);  // solid on
      }
      break;

    case LED_SOLID:
      if (now - ledPhaseStart >= SOLID_PHASE_MS) {
        ledState = LED_DONE;
        digitalWrite(LED_PIN, LOW);  // off
      }
      break;

    case LED_DONE:
      break;
  }
}

static void sendAT(const char *cmd, unsigned long waitMs = 300) {
  Serial1.print(cmd);
  Serial1.print("\r\n");
  unsigned long start = millis();
  while (millis() - start < waitMs) {
    while (Serial1.available()) Serial.write(Serial1.read());
  }
}

static void configureHM10() {
  delay(500);
  while (Serial1.available()) Serial1.read();

  Serial.println("Configuring HM-10...");
  sendAT("AT", 400);
  sendAT("AT+NAME=Intelliwave", 400);
  sendAT("AT+PASS=123456", 400);
  sendAT("AT+PIO10", 400); 
  sendAT("AT+RESET", 700);
  Serial.println("HM-10 configuration done.");
}

static void sendPacketBinary() {
  uint8_t buf[FRAME_LEN];
  buf[0] = SYNC1;
  buf[1] = SYNC2;
  buf[2] = packetCounter;

  uint8_t sum = packetCounter;
  for (uint8_t i = 0; i < PACKET_SAMPLES; i++) {
    int16_t v = packetBuf[i];
    uint8_t lo = (uint8_t)(v & 0xFF);
    uint8_t hi = (uint8_t)((v >> 8) & 0xFF);
    buf[3 + i * 2]     = lo;
    buf[3 + i * 2 + 1] = hi;
    sum = (uint8_t)(sum + lo + hi);
  }
  buf[FRAME_LEN - 1] = sum;

  Serial1.write(buf, FRAME_LEN);

  
  // Serial.write(buf, FRAME_LEN);

  packetCounter++;  
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);   

  pinMode(SDN_PIN, OUTPUT);
  digitalWrite(SDN_PIN, HIGH);
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledPhaseStart = millis();
  lastBlinkToggle = millis();

  configureHM10();
}

void loop() {
  updateLed();

  unsigned long now = micros();
  if (now - lastTime >= interval_us) {
    lastTime = now;
    int raw = analogRead(ECG_PIN);

    if (!baselineInit) {
      baseline = raw;
      baselineInit = true;
    } else {
      baseline = alphaDC * raw + (1 - alphaDC) * baseline;
    }

    int centered = raw - (int)baseline;
    packetBuf[packetIdx++] = (int16_t)centered;
    sampleCount++;

    if (packetIdx >= PACKET_SAMPLES) {
      sendPacketBinary();
      packetIdx = 0;
    }
  }
}