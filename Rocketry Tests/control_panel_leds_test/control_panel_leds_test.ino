#include <Adafruit_NeoPixel.h>

#define NUM_PIXELS   4
#define NUM_SWITCHES 8
#define NUM_STRIPS   2

const int switchPins[NUM_SWITCHES] = {37,35,33,31,29,27,25,23};//{23,25,27,29,31,33,35,37};

#define LED_PIN1 38
#define LED_PIN2 39

Adafruit_NeoPixel leds[NUM_STRIPS] = {
  Adafruit_NeoPixel(NUM_PIXELS, LED_PIN1, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_PIXELS, LED_PIN2, NEO_GRB + NEO_KHZ800)
};

bool ledState[NUM_SWITCHES] = {false};

void setup() {
  for (int s = 0; s < NUM_STRIPS; s++) {
    leds[s].begin();
    leds[s].clear();
    leds[s].show();
  }

  for (int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(switchPins[i], INPUT_PULLUP);
  }
}

void loop() {

  // Read switches correctly
  for (int i=0; i<NUM_SWITCHES; i++) {
    ledState[i] = (digitalRead(switchPins[i]) == LOW);
  }

  // Update LEDs
  for (int i=0; i<NUM_SWITCHES; i+=2) {
    int strip = i / 4;
    int pixel = i % 4;

    if (ledState[i]) {    //check if armed
      leds[strip].setPixelColor(pixel,     leds[strip].Color(255, 0, 0));
      if (ledState[i+1]) {    //check if valve needs to be moved
        leds[strip].setPixelColor(pixel + 1, leds[strip].Color(0, 255, 0));
      } else {
        leds[strip].setPixelColor(pixel + 1, leds[strip].Color(255, 0, 0));
      }
    } else {    //or de-arm
      leds[strip].setPixelColor(pixel,     leds[strip].Color(0, 0, 0));
    }
  }

  // PUSH data to LEDs
  for (int s = 0; s < NUM_STRIPS; s++) {
    leds[s].show();
  }
}