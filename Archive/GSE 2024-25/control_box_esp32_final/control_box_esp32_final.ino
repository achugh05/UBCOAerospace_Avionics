#define DEBOUNCE_MS 50

// Index order: 0 = Main, 1 = Center, 2 = Booster, 3 = Purge, 4 = Test
const uint8_t BTN_PINS[5] = {27, 32, 18, 23, 18};
const uint8_t LED_R[4]    = {13, 26, 15, 19};  // Red pins
const uint8_t LED_G[4]    = {12, 25,  2, 21};  // Green pins
const uint8_t LED_B[4]    = {14, 33,  5, 22};  // Blue pins

uint8_t lastState[5] = {0};
uint32_t lastTime[5] = {0};

HardwareSerial loraSerial(2);  // Serial2: RX = 16, TX = 17

void setup() {
  Serial.begin(115200);
  loraSerial.begin(9600, SERIAL_8N1, 16, 17);

  for (int i = 0; i < 5; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLDOWN);
    if (i < 4) {
      pinMode(LED_R[i], OUTPUT);
      pinMode(LED_G[i], OUTPUT);
      pinMode(LED_B[i], OUTPUT);
      digitalWrite(LED_R[i], LOW);
      digitalWrite(LED_G[i], LOW);
      digitalWrite(LED_B[i], LOW);
    }
  }

  Serial.println("Control Box ESP32 started");
}

void loop() {
  for (int i = 0; i < 5; i++) {
    checkButton(i, BTN_PINS[i]);
  }

  handleIncomingLoRaPacket();
}

void checkButton(uint8_t idx, uint8_t pin) {
  uint8_t current = digitalRead(pin);
  if (current != lastState[idx] && (millis() - lastTime[idx] > DEBOUNCE_MS)) {
    lastTime[idx] = millis();
    lastState[idx] = current;
    if (current == HIGH) {
      Serial.printf("Button %d pressed\n", idx + 1);
      sendButtonPressed(idx);
    }
  }
}

void sendButtonPressed(uint8_t buttonID) {
  uint8_t commandChar;
  switch (buttonID) {
    case 0: commandChar = 'M'; break;  // Main
    case 1: commandChar = 'C'; break;  // Center
    case 2: commandChar = 'B'; break;  // Booster
    case 3: commandChar = 'P'; break;  // Purge
    case 4: commandChar = 'I'; break;  // Test (new button)
    default: return;
  }

  uint8_t packet[4] = {0xAA, commandChar, commandChar, 0xFF};
  loraSerial.write(packet, 4);

  Serial.print("Sent packet: ");
  for (uint8_t i = 0; i < 4; i++) Serial.printf("%02X ", packet[i]);
  Serial.println();
}

void handleIncomingLoRaPacket() {
  static uint8_t buffer[12];
  static uint8_t index = 0;

  while (loraSerial.available()) {
    uint8_t byteIn = loraSerial.read();

    if (index == 0 && byteIn != 0xAB) {
      continue;  // Wait for start byte
    }

    buffer[index++] = byteIn;

    if (index == 12) {
      index = 0;

      Serial.print("Received UART packet: ");
      for (int i = 0; i < 12; i++) Serial.printf("%02X ", buffer[i]);
      Serial.println();

      if (buffer[0] == 0xAB && buffer[11] == 0xEF) {
        uint8_t states[4] = {
          buffer[1], buffer[2], buffer[3], buffer[4]
        };

        Serial.print("Parsed valve states: ");
        for (int i = 0; i < 4; i++) Serial.printf("%c ", states[i]);
        Serial.println();

        applyValveLEDs(states);
      } else {
        Serial.println("Invalid packet header/footer");
      }
    }
  }
}

void applyValveLEDs(uint8_t states[4]) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_R[i], LOW);
    digitalWrite(LED_G[i], LOW);
    digitalWrite(LED_B[i], LOW);

    switch (states[i]) {
      case 'o': digitalWrite(LED_G[i], HIGH); break;
      case 'c': digitalWrite(LED_R[i], HIGH); break;
      case 'd': digitalWrite(LED_B[i], HIGH); break;
      case 'm':
        digitalWrite(LED_G[i], HIGH);
        digitalWrite(LED_B[i], HIGH);
        digitalWrite(LED_R[i], HIGH);
        break;
    }
  }
}
