#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// W2 integrated bench prototype: ring is temporarily powered from 3V3.
// All 16 pixels breathe dimly; pressure adds a brighter 1-4 pixel response.
const byte DRV = 0x5A;
const byte LIS = 0x19;
const int FSR_PIN = 4;
const int BUTTON_PIN = 5;
const int LED_PIN = 6;
const int LED_COUNT = 16;
const int PRESS_ON = 50;
const int PRESS_OFF = 20;
const unsigned long PULSE_MS = 300;
const unsigned long MOTION_PULSE_MS = 600;
const unsigned long MOTION_PULSE_GAP_MS = 250;
const unsigned long COOLDOWN_MS = 700;
const unsigned long MOTION_COOLDOWN_MS = 8000;
const unsigned long MOTION_REARM_QUIET_MS = 2000;
const int MOTION_THRESHOLD = 8000;
const int MOTION_QUIET_THRESHOLD = 1000;
const unsigned long BREATH_MS = 4000;
const unsigned long LIGHT_FRAME_MS = 25;

Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

bool drvReady = false, lisReady = false, enabled = true;
bool pressureHeld = false, motorOn = false, lastButton = HIGH;
bool motionPulse = false;
bool secondMotionPulsePending = false;
unsigned long secondMotionPulseAt = 0;
bool havePreviousMotion = false;
bool motionArmed = true;
int16_t previousX = 0, previousY = 0, previousZ = 0;
unsigned long lastLightFrame = 0;
long lastMotionChange = 0;
bool lastMotionReadOK = false;
unsigned long lastButtonChange = 0, pulseStart = 0, lastPulseEnd = 0;
unsigned long lastMotionPulse = 0, lastPrint = 0, lastMotionRead = 0;
unsigned long motionQuietSince = 0;

void writeReg(byte addr, byte reg, byte value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

byte readReg(byte addr, byte reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (byte)1);
  return Wire.available() ? Wire.read() : 0;
}

bool readMotion(int16_t &x, int16_t &y, int16_t &z) {
  Wire.beginTransmission(LIS);
  Wire.write(0x28 | 0x80);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(LIS, (byte)6) != 6) return false;
  x = (int16_t)(Wire.read() | (Wire.read() << 8));
  y = (int16_t)(Wire.read() | (Wire.read() << 8));
  z = (int16_t)(Wire.read() | (Wire.read() << 8));
  return true;
}

void showLight(int level, bool active, unsigned long now) {
  if (now - lastLightFrame < LIGHT_FRAME_MS) return;
  lastLightFrame = now;
  const float phase = (now % BREATH_MS) * (2.0f * PI / BREATH_MS);
  const float envelope = 0.5f - 0.5f * cosf(phase);
  const uint8_t red = active ? 2 + (uint8_t)(18 * envelope) : 0;
  const uint8_t green = active ? 1 + (uint8_t)(5 * envelope) : 0;
  for (int i = 0; i < LED_COUNT; i++) {
    // Pressure accents the first 1-4 pixels while retaining the soft breath.
    ring.setPixelColor(i, i < level ? ring.Color(48, 12, 0)
                                    : ring.Color(red, green, 0));
  }
  ring.show();
}

void stopMotor() {
  if (!motorOn) return;
  writeReg(DRV, 0x02, 0);
  motorOn = false;
  lastPulseEnd = millis();
  if (motionPulse && secondMotionPulsePending && enabled) {
    secondMotionPulseAt = lastPulseEnd + MOTION_PULSE_GAP_MS;
  }
  Serial.println("MOTOR OFF");
}

void startPulse(const char *reason) {
  if (!drvReady || !enabled || motorOn) return;
  writeReg(DRV, 0x02, 0x7F);
  motorOn = true;
  motionPulse = reason[0] == 'M' && reason[1] == 'O';
  pulseStart = millis();
  Serial.print("MOTOR ON: ");
  Serial.println(reason);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  ring.begin();
  ring.setBrightness(255); // RGB values above already cap actual brightness
  ring.clear();
  ring.show();
  Wire.begin(8, 9);
  delay(500);

  Wire.beginTransmission(DRV);
  drvReady = Wire.endTransmission() == 0;
  if (drvReady) {
    writeReg(DRV, 0x02, 0);
    writeReg(DRV, 0x1A, readReg(DRV, 0x1A) & 0x7F); // ERM motor
    writeReg(DRV, 0x01, 0x05); // real-time playback
  }

  lisReady = readReg(LIS, 0x0F) == 0x33;
  if (lisReady) {
    writeReg(LIS, 0x20, 0x57); // 100 Hz, X/Y/Z enabled
    writeReg(LIS, 0x23, 0x88); // high resolution, +/-2g
  }

  Serial.print("DRV: "); Serial.println(drvReady ? "OK" : "NOT FOUND");
  Serial.print("LIS3DH: "); Serial.println(lisReady ? "OK" : "NOT FOUND");
  Serial.println("W2 INTEGRATED READY: 4s breath + pressure light/motor + motion; button pauses.");
}

void loop() {
  unsigned long now = millis();
  int pressure = analogRead(FSR_PIN);
  bool button = digitalRead(BUTTON_PIN);

  if (button != lastButton && now - lastButtonChange > 40) {
    lastButtonChange = now;
    lastButton = button;
    if (button == LOW) {
      enabled = !enabled;
      if (!enabled) {
        secondMotionPulsePending = false;
        stopMotor();
      }
      Serial.println(enabled ? "ENABLED" : "PAUSED");
    }
  }

  // Pressure stays visible while held. The four stages are easy to tune from
  // the pressure values printed in Serial Monitor.
  int lightLevel = 0;
  if (enabled && pressure > PRESS_ON) {
    lightLevel = 1;
    if (pressure > 250) lightLevel = 2;
    if (pressure > 800) lightLevel = 3;
    if (pressure > 1600) lightLevel = 4;
  }
  showLight(lightLevel, enabled, now);

  if (pressure < PRESS_OFF) pressureHeld = false;
  if (enabled && !pressureHeld && pressure > PRESS_ON && now - lastPulseEnd > COOLDOWN_MS) {
    secondMotionPulsePending = false;
    pressureHeld = true;
    startPulse("PRESSURE");
  }

  if (lisReady && now - lastMotionRead >= 100) {
    lastMotionRead = now;
    int16_t x, y, z;
    if (readMotion(x, y, z)) {
      lastMotionReadOK = true;
      if (havePreviousMotion) {
        long change = labs((long)x - previousX) + labs((long)y - previousY) + labs((long)z - previousZ);
        lastMotionChange = change;
        // A continuous move must settle before another motion response is armed.
        if (!motionArmed) {
          if (change < MOTION_QUIET_THRESHOLD) {
            if (motionQuietSince == 0) motionQuietSince = now;
          } else {
            motionQuietSince = 0;
          }
          if (motionQuietSince != 0 && now - motionQuietSince >= MOTION_REARM_QUIET_MS &&
              now - lastMotionPulse >= MOTION_COOLDOWN_MS) {
            motionArmed = true;
            Serial.println("MOTION REARMED");
          }
        }
        if (enabled && motionArmed && !motorOn && pressure < PRESS_OFF && now > 2000 &&
            change > MOTION_THRESHOLD &&
            now - lastPulseEnd > COOLDOWN_MS) {
          lastMotionPulse = now;
          motionArmed = false;
          motionQuietSince = 0;
          Serial.print("MOTION change="); Serial.println(change);
          secondMotionPulsePending = true;
          secondMotionPulseAt = 0;
          startPulse("MOTION");
        }
      }
      previousX = x; previousY = y; previousZ = z;
      havePreviousMotion = true;
    } else {
      lastMotionReadOK = false;
    }
  }

  if (motorOn && now - pulseStart >= (motionPulse ? MOTION_PULSE_MS : PULSE_MS)) stopMotor();
  if (secondMotionPulsePending && secondMotionPulseAt != 0 && !motorOn &&
      now >= secondMotionPulseAt) {
    secondMotionPulsePending = false;
    secondMotionPulseAt = 0;
    if (enabled && pressure < PRESS_ON) startPulse("MOTION 2");
  }
  if (Serial.available() && Serial.read() == 'v') startPulse("MANUAL");
  if (now - lastPrint >= 500) {
    lastPrint = now;
    Serial.print("pressure="); Serial.print(pressure);
    Serial.print(" light="); Serial.print(lightLevel);
    Serial.print(" lis="); Serial.print(lisReady ? (lastMotionReadOK ? "OK" : "READ_ERR") : "NOT_FOUND");
    Serial.print(" motion_change="); Serial.print(lastMotionChange);
    Serial.print(" mode="); Serial.println(enabled ? "ENABLED" : "PAUSED");
  }
}
