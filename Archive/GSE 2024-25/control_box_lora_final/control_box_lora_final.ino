#include <SPI.h>
#include <RadioLib.h>
#include <TM1637Display.h>

// LoRa Pins
#define NSS    8
#define DIO1   14
#define RESET  12
#define BUSY   13

SX1262 radio = new Module(NSS, DIO1, RESET, BUSY);
HardwareSerial espSerial(2);  // Serial2 for UART to Control Box ESP32

const uint8_t TX_PACKET_LENGTH = 4;
const uint8_t RX_PACKET_LENGTH = 12;

uint8_t txPacket[TX_PACKET_LENGTH];
uint8_t rxPacket[RX_PACKET_LENGTH];

// Tank display (CLK=7, DIO=6)
#define TANK_CLK 7
#define TANK_DIO 6
TM1637Display tankDisplay(TANK_CLK, TANK_DIO);

// Main display (CLK=5, DIO=4)
#define MAIN_CLK 5
#define MAIN_DIO 4
TM1637Display mainDisplay(MAIN_CLK, MAIN_DIO);

// Booster display (CLK=39, DIO=40)
#define BOOSTER_CLK 39
#define BOOSTER_DIO 40
TM1637Display boosterDisplay(BOOSTER_CLK, BOOSTER_DIO);

// Center display (CLK=41, DIO=42)
#define CENTER_CLK 41
#define CENTER_DIO 42
TM1637Display centerDisplay(CENTER_CLK, CENTER_DIO);

void setup() {
  Serial.begin(115200);
  espSerial.begin(9600, SERIAL_8N1, 48, 47); // ESP RX=48, TX=47
  SPI.begin(9, 11, 10, NSS);

  if (radio.begin(900.0, 125.0, 10, 8, 0x12, 22) != RADIOLIB_ERR_NONE) {
    Serial.println("LoRa init failed!");
    while (true);
  }

  Serial.println("Control Box LoRa started");

  delay(500);
  while (espSerial.available()) espSerial.read();  // flush junk

  // Initialize displays
  tankDisplay.setBrightness(0x0f);
  mainDisplay.setBrightness(0x0f);
  boosterDisplay.setBrightness(0x0f);
  centerDisplay.setBrightness(0x0f);
}

void loop() {
  handleUartTransmit();
  handleLoRaReceive();
}

// -------------------- Transmit 4-byte UART Packet --------------------
void handleUartTransmit() {
  if (espSerial.available() >= TX_PACKET_LENGTH) {
    for (uint8_t i = 0; i < TX_PACKET_LENGTH; i++) {
      txPacket[i] = espSerial.read();
    }

    Serial.print("Transmitting LoRa packet: ");
    for (uint8_t i = 0; i < TX_PACKET_LENGTH; i++) {
      Serial.printf("%02X ", txPacket[i]);
    }
    Serial.println();

    for (int i = 0; i < 5; i++) {
      radio.standby();  // Exit receive mode
      int state = radio.transmit(txPacket, TX_PACKET_LENGTH);
      if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("LoRa packet sent (copy %d)\n", i + 1);
      } else {
        Serial.printf("LoRa send failed (copy %d), code: %d\n", i + 1, state);
      }
      delay(100);  // 50 ms between retries
      radio.startReceive();  // Resume RX mode after each transmit
    }

    espSerial.flush();
    while (espSerial.available()) espSerial.read();  // Flush excess
  }
}

// -------------------- Receive 12-byte LoRa Packet --------------------
void handleLoRaReceive() {
  int recvState = radio.receive(rxPacket, RX_PACKET_LENGTH);

  if (recvState == RADIOLIB_ERR_NONE) {
    Serial.println(radio.available());
    if (rxPacket[0] == 0xAB && rxPacket[11] == 0xEF) {
      Serial.print("LoRa packet received: ");
      for (int i = 0; i < RX_PACKET_LENGTH; i++) {
        Serial.printf("%02X ", rxPacket[i]);
      }
      Serial.println();

      // Forward valve states (bytes 1 to 4) over UART
      espSerial.write(rxPacket, 12);

      // Optionally update displays
      updateDisplaysFromPacket(rxPacket);
    } else {
      Serial.println("Invalid LoRa packet (wrong header/footer)");
    }
  } else if (recvState != RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.print("Receive error: ");
    Serial.println(recvState);
  }
}

// -------------------- Update 7-segment Displays --------------------
void updateDisplaysFromPacket(uint8_t packet[12]) {
  tankDisplay.showNumberDec(packet[6] * 4, true);
  mainDisplay.showNumberDec(packet[7] * 4, true);
  boosterDisplay.showNumberDec(packet[8] * 4, true);
  centerDisplay.showNumberDec(packet[9] * 4, true);
  Serial.println((packet[6]*4));
  
}
