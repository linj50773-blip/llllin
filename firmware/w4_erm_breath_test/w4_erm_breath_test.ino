#include <Wire.h>
#include <math.h>

// W4 tactile breath test. Existing GPIO8/9 I2C -> DRV2605L -> 1027 ERM.
// Send b + Enter to start; q + Enter to stop. Auto-stops after 5 minutes.
constexpr uint8_t DRV_ADDR = 0x5A;
constexpr uint8_t RTP_MIN = 40;
constexpr uint8_t RTP_MAX = 55;
constexpr uint32_t CYCLE_MS = 4000;
constexpr uint32_t FRAME_MS = 25;
constexpr uint32_t MAX_RUN_MS = 300000;
bool drvReady = false;
bool breathing = false;
uint32_t breathStarted = 0;
uint32_t lastFrame = 0;
uint32_t lastCycleReport = 0;

void writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(DRV_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(DRV_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom(DRV_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

void stopBreath() {
  breathing = false;
  writeReg(0x02, 0);
  Serial.println("W4 BREATH OFF");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);
  delay(500);
  Wire.beginTransmission(DRV_ADDR);
  drvReady = Wire.endTransmission() == 0;
  if (!drvReady) {
    Serial.println("DRV NOT FOUND at 0x5A");
    return;
  }
  writeReg(0x02, 0);
  writeReg(0x1A, readReg(0x1A) & 0x7F); // ERM
  writeReg(0x01, 0x05);                 // real-time playback
  Serial.println("W4 BREATH READY | b=start q=stop | RTP 40..55 | 4s cycle");
}

void loop() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (!drvReady) continue;
    if (c == 'b') {
      breathStarted = millis();
      lastFrame = 0;
      lastCycleReport = 0;
      breathing = true;
      Serial.println("W4 BREATH ON");
    } else if (c == 'q') {
      stopBreath();
    }
  }
  if (!breathing) return;
  const uint32_t now = millis();
  const uint32_t elapsed = now - breathStarted;
  if (elapsed >= MAX_RUN_MS) {
    stopBreath();
    Serial.println("W4 AUTO STOP 5 MIN");
    return;
  }
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    const float phase = (elapsed % CYCLE_MS) * (2.0f * PI / CYCLE_MS);
    const float envelope = 0.5f - 0.5f * cosf(phase);
    const uint8_t rtp = RTP_MIN + (uint8_t)((RTP_MAX - RTP_MIN) * envelope + 0.5f);
    writeReg(0x02, rtp);
  }
  if (elapsed / CYCLE_MS != lastCycleReport) {
    lastCycleReport = elapsed / CYCLE_MS;
    Serial.print("W4 CYCLE "); Serial.println(lastCycleReport);
  }
}
