
#include <SPI.h>
#include <RadioLib.h>
#include <ESP32Servo.h>

#define NSS    8
#define DIO1   14
#define RESET  12
#define BUSY   13

// Limit switch pins
#define MAIN_CLOSED   48
#define MAIN_OPEN     47
#define CENTER_CLOSED 33
#define CENTER_OPEN   34
#define BOOSTER_CLOSED 39
#define BOOSTER_OPEN   40
#define PURGE_CLOSED   41
#define PURGE_OPEN     42

#define IGINATION 26

// Pressure sensor pins
const int pressurePins[4] = {7, 6, 5, 4};

// Calibration
const int V1_mV = 1527;
const int V2_mV = 8800;
const float P1 = 0.0;
const float P2 = 750.0;

float A = 1.0, B = 0.0;
unsigned long lastTransmit = 0;
const unsigned long transmitInterval = 1000;

SX1262 radio = new Module(NSS, DIO1, RESET, BUSY);

// Servo objects
Servo mainServo, centerServo, boosterServo, purgeServo;
int mainAngle = 0, centerAngle = 0, boosterAngle = 0, purgeAngle = 0;

void setup() {
  Serial.begin(115200);
  SPI.begin(9, 11, 10, NSS);

  pinMode(MAIN_CLOSED, INPUT_PULLUP);
  pinMode(MAIN_OPEN, INPUT_PULLUP);
  pinMode(CENTER_CLOSED, INPUT_PULLUP);
  pinMode(CENTER_OPEN, INPUT_PULLUP);
  pinMode(BOOSTER_CLOSED, INPUT_PULLUP);
  pinMode(BOOSTER_OPEN, INPUT_PULLUP);
  pinMode(PURGE_CLOSED, INPUT_PULLUP);
  pinMode(PURGE_OPEN, INPUT_PULLUP);

  pinMode(IGINATION, OUTPUT);

  mainServo.attach(2);
  centerServo.attach(3);
  boosterServo.attach(36);
  purgeServo.attach(35);



  mainServo.write(60);
  centerServo.write(60);
  boosterServo.write(60);
  purgeServo.write(60);

  if (radio.begin(900.0, 125.0, 10, 8, 0x12, 22) != RADIOLIB_ERR_NONE) {
    Serial.println("LoRa init failed!");
    while (true);
  }

  if (V2_mV != V1_mV) {
    A = (P2 - P1) / (V2_mV - V1_mV);
    B = P1 - A * V1_mV;
  } else {
    while (true);
  }

}

void loop() {
  if (millis() - lastTransmit >= transmitInterval) {
    lastTransmit = millis();
    sendPacket();
  }

  handleReceive();
}

char getValveState(uint8_t closedPin, uint8_t openPin) {
  bool closed = digitalRead(closedPin);
  bool open = digitalRead(openPin);
  if (!closed && open) return 'c';
  if (closed && !open) return 'o';
  if (!closed && !open) return 'm';
  return 'd';
}

void readPressure(uint8_t* out) {
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(pressurePins[i]);
    long mV = (raw * 5000L) / 1023;
    float psi = mV;
    if (psi < 0) psi = 0;
    float scaled = (psi / 60.0) * 255.0;
    if (scaled > 255) scaled = 255;
    out[i] = (uint8_t)psi;
    Serial.println(psi);
  }
}

void buildPacket(uint8_t* packet) {
  packet[0] = 0xAB;

  packet[1] = getValveState(MAIN_CLOSED, MAIN_OPEN);
  packet[2] = getValveState(CENTER_CLOSED, CENTER_OPEN);
  packet[3] = getValveState(BOOSTER_CLOSED, BOOSTER_OPEN);
  packet[4] = getValveState(PURGE_CLOSED, PURGE_OPEN);

  packet[5] = packet[1] + packet[2] + packet[3] + packet[4];

  uint8_t pressures[4];
  readPressure(pressures);
  for (int i = 0; i < 4; i++) {
    packet[6 + i] = pressures[i]/4;
  }

  packet[10] = pressures[0] + pressures[1] + pressures[2] + pressures[3];
  packet[11] = 0xEF;
}

void sendPacket() {
  uint8_t txPacket[12];
  buildPacket(txPacket);

  Serial.print("Sending packet: ");
  for (int i = 0; i < sizeof(txPacket); i++) {
    Serial.printf("%02X ", txPacket[i]);
  }
  Serial.println();

  radio.standby(); // Exit receive mode
  int err = radio.transmit(txPacket, sizeof(txPacket));
  if (err == RADIOLIB_ERR_NONE) {
    Serial.println("Telemetry sent");
  } else {
    Serial.printf("Telemetry failed: %d\n", err);
  }
  radio.startReceive(); // Resume receive mode
}


void toggleServo(Servo& servo, int& angle) {
  if (angle == 0) {
    angle = 60;
  } else {
    angle = 0;
  }
  servo.write(angle);
}

unsigned long lastCommandTime = 0;
char lastCommand = 0;

unsigned long lastActionTime = 0;
const unsigned long ACTION_COOLDOWN = 2000;  // 2 seconds

void handleReceive() {
  uint8_t rxPacket[12];
  int state = radio.receive(rxPacket, sizeof(rxPacket));

  if (state == RADIOLIB_ERR_NONE) {
    Serial.print("Received: ");
    for (int i = 0; i < 12; i++) {
      Serial.print(rxPacket[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    unsigned long now = millis();

    // If still within cooldown period, ignore this packet
    if (now - lastActionTime < ACTION_COOLDOWN) {
      Serial.println("In cooldown, ignoring command.");
      return;
    }

    // Proceed with acting on the command
    char command = rxPacket[1];
    Serial.println("Executing command");

    switch (command) {
      case 'M': toggleServo(mainServo, mainAngle); break;
      case 'C': toggleServo(centerServo, centerAngle); break;
      case 'B': toggleServo(boosterServo, boosterAngle); break;
      case 'P': toggleServo(purgeServo, purgeAngle); break;
      case 'I':
        digitalWrite(IGINATION, HIGH);
        delay(1000);
        digitalWrite(IGINATION, LOW);
        delay(1000);
        break;
    }

    // Set cooldown timer
    lastActionTime = now;
  }
}
