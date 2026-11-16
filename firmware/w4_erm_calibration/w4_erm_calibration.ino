#include <Wire.h>

// Interactive W4 ERM calibration for the existing ESP32-S3 + DRV2605L wiring.
// Send an integer 0..127 followed by New Line in Serial Monitor.
// The motor runs at that RTP level for 1.5 seconds, then stops automatically.
constexpr uint8_t DRV_ADDR = 0x5A;
constexpr uint32_t TEST_MS = 1500;
bool motorOn = false;
uint32_t motorStarted = 0;
String input;

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(DRV_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(DRV_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom(DRV_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

void stopMotor() {
  writeReg(0x02, 0);
  motorOn = false;
  Serial.println("RTP OFF");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);
  delay(500);
  Wire.beginTransmission(DRV_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("DRV NOT FOUND at 0x5A");
    return;
  }
  writeReg(0x02, 0);
  writeReg(0x1A, readReg(0x1A) & 0x7F); // ERM, not LRA
  writeReg(0x01, 0x05);                 // real-time playback
  Serial.println("W4 ERM READY | send 0..127 then Enter | 1.5s per test");
}

void loop() {
  if (motorOn && millis() - motorStarted >= TEST_MS) stopMotor();
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\r' || c == '\n') {
      input.trim();
      if (input.length()) {
        bool valid = true;
        for (unsigned int i = 0; i < input.length(); ++i) {
          if (!isDigit(input[i])) valid = false;
        }
        const int level = input.toInt();
        if (valid && level >= 0 && level <= 127) {
          writeReg(0x02, (uint8_t)level);
          motorOn = level > 0;
          motorStarted = millis();
          Serial.print("RTP="); Serial.println(level);
        } else {
          Serial.println("Use a whole number 0..127");
        }
      }
      input = "";
    } else if (input.length() < 4) {
      input += c;
    }
  }
}
