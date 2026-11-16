#include <Wire.h>

void scanBus() {
  Serial.println("TIDE W1 I2C scan | SDA GPIO8 | SCL GPIO9");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found 0x");
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
    }
  }
  Serial.println("Scan complete");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9); // Actual Tide wiring: SDA=GPIO8, SCL=GPIO9
  delay(1500);
}

void loop() {
  scanBus();
  delay(5000);
}
