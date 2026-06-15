#include <TM1637Display.h>

const int dio_pins[] = {28, 22, 26, 24};
const int clk_pin = 30;

TM1637Display display0(clk_pin, dio_pins[0]);
TM1637Display display1(clk_pin, dio_pins[1]);
TM1637Display display2(clk_pin, dio_pins[2]);
TM1637Display display3(clk_pin, dio_pins[3]);

TM1637Display displays[] = {display0, display1, display2, display3};
int counter = 0;

void setup() {
  Serial.begin(115200);
  for (int i=0; i<4; i++) {
    displays[i].setBrightness(0x0f);
    displays[i].showNumberDec(1234, true);
    Serial.println("Display " + String(i) + " turned on.");
  }
  delay(1000);
}

void loop() {
  for (int i=0; i<4; i++) {
    delay(250);
    displays[i].showNumberDec(counter + i, true);
  }
  counter += 4;
  Serial.println(counter);
}
