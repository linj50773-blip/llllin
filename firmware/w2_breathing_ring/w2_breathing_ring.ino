#include <Adafruit_NeoPixel.h>
#include <math.h>

// W2 bench test. Use the existing 3V3 supply and GPIO6 -> 330 ohm -> DIN.
// Low RGB values keep a 16-pixel ring gentle and reduce load on ESP32 3V3.
constexpr uint8_t LED_PIN = 6;
constexpr uint8_t LED_COUNT = 16;
constexpr uint32_t BREATH_MS = 4000;
constexpr uint32_t FRAME_MS = 25;
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
uint32_t lastFrame = 0;
uint32_t lastReport = 0;

void setup() {
  Serial.begin(115200);
  ring.begin();
  ring.setBrightness(255);
  ring.clear();
  ring.show();
  Serial.println("W2 BREATH READY | 16 LEDs | GPIO6 | 4s cycle");
}

void loop() {
  const uint32_t now = millis();
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    const float phase = (now % BREATH_MS) * (2.0f * PI / BREATH_MS);
    const float envelope = 0.5f - 0.5f * cosf(phase);
    const uint8_t red = 2 + (uint8_t)(18 * envelope);
    const uint8_t green = 1 + (uint8_t)(5 * envelope);
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
      ring.setPixelColor(i, ring.Color(red, green, 0));
    }
    ring.show();
  }
  if (now - lastReport >= BREATH_MS) {
    lastReport = now;
    Serial.println("W2 BREATH CYCLE");
  }
}
