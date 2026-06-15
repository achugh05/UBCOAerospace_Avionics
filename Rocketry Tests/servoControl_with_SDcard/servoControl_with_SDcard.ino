#include <ESP32Servo.h>

#define SERVO_PIN 25     // NOT 35 – must be an output pin
Servo myServo;
int servoAngle = 90;     // start centered

#include <SPI.h>    //micro sd card is wired in SPI
#include <SD.h>

#define CS_PIN 5    //select SPI pin

File dataFile;

const int limitPin = 13;      //limit switch pin
int count1 = 0;

bool lastState = HIGH;

unsigned long debounce1 = 0;      //used for debouncing the limit switch button (holding pressed will not register as additional presses)
unsigned long lastDebounceTime = 0;
const unsigned long debounce = 10;


void setup() {
  // put your setup code here, to run once:
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting the magnanimous SD logging system.");

  if (!SD.begin(CS_PIN)) {      //check if SD module can be detected
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully.");

  dataFile = SD.open("/data327.csv", FILE_WRITE);     //MUST USE "/" BEFORE FILE NAME WITH ESP32
  // FILE_WRITE REWRITES. FILE_APPEND CONTINUES TO ADD TO THE FILE.
  if (!dataFile) {
    Serial.println("Error opening file!");
    return;
  }

  Serial.println("Your file has been found!!!");
  dataFile.println("Starting log.");
  
  pinMode(limitPin, INPUT_PULLUP);
  myServo.attach(SERVO_PIN);
  myServo.write(servoAngle);
  Serial.println("Servo ready. Use keys a/d/s or 0-9.");

}

void loop() {
  // put your main code here, to run repeatedly:
  handleServoKeyboard();
  int limitSwitch = digitalRead(limitPin);

    if (limitSwitch == LOW) {
    if (lastState == HIGH && millis() - debounce1 > debounce) {     //debouncing logic
      Serial.print("press1 ");
      dataFile.print("press1 ");
      Serial.println(count1);
      dataFile.println(count1);
      lastState = LOW;
      count1++;
      debounce1 = 0;
    }
  } else {
    lastState = HIGH;
    debounce1 = millis();
  }

  dataFile.flush(); //ensure data is written and not in buffer
}


void handleServoKeyboard() {
  if (Serial.available()) {
    char key = Serial.read();

    if (key == 'a') {
      servoAngle -= 5;
    }
    else if (key == 'd') {
      servoAngle += 5;
    }
    else if (key == 's') {
      servoAngle = 90;
    }
    else if (key >= '0' && key <= '9') {
      servoAngle = map(key - '0', 0, 9, 0, 180);
    }

    servoAngle = constrain(servoAngle, 0, 180);
    myServo.write(servoAngle);

    Serial.print("Servo angle: ");
    Serial.println(servoAngle);
  }
}
