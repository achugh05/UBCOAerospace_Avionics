#include <Adafruit_NeoPixel.h>

#define LED_PIN 44
#define NUM_PIXELS 4

Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.clear();
  strip.show();
}

void loop() {

  for (int i=0; i< NUM_PIXELS;i++) {
  strip.clear();
  strip.setPixelColor(i, 0, 255, 0);
  strip.show();
  delay(1000);
  }
    for (int i=0; i< NUM_PIXELS;i++) {
  strip.clear();
  strip.setPixelColor(i, 0, 0, 0);
  strip.show();
  delay(1000);
  }
}