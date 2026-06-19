#include <RadioLib.h>

HardwareSerial loraSerial(1);   //UART bus declaration

// ================== common - RADIO CONFIG ==================
#define NSS    8
#define DIO1   14
#define RESET  12
#define BUSY   13
SX1262 radio = new Module(NSS, DIO1, RESET, BUSY);

// ================== SYSTEM CONFIG ==================
#define RX_LORA 19
#define TX_LORA 20

// transmission constants
#define HEADER 0xAB
#define VERSION 1
#define DEVICE_ID 3
#define FOOTER 0xEF

// common - computes the CRC8 byte for a given array
uint8_t computeCRC8(uint8_t* data, int length) {
  uint8_t crc = 0x00;

  for (int i = 0; i < length; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
    }
  }
  return crc;
}

// ================== LOGGING =============================
// common for loras - used to give an explanation for events in the log
void logEvent(String message) {
  Serial.println(message);    //for debugging only
}

// common - prints packets to Serial monitor for use in debugging. 
void printPacket(uint8_t* packet, int telemetryLength) {
  for (int i=0; i<telemetryLength; i++) {
    Serial.print(packet[i]);
    Serial.print(" ");
  }
  Serial.println();
}

// ---------------- PACKET VALIDATION ----------------
// common - checks if the header, version, footer, or CRC byte are correct. Does not check device ID
bool checkPacketValidity(uint8_t* packet, int length) {
  uint8_t receivedCRC = packet[length - 2];
  uint8_t calculatedCRC = computeCRC8(packet, length - 2);
  if (packet[0] != HEADER || packet[1] != VERSION || packet[length - 1] != FOOTER || receivedCRC != calculatedCRC) {
    return false;
  } else {
    return true;
  }
}

// ---------------- RECEIVE ----------------
// common - receive data over radio, parse
void handleRadioReceive() {
  uint8_t buffer[32];	
  int recvState = radio.receive(buffer, 32);

  if (recvState == RADIOLIB_ERR_NONE) {
    size_t actualLength = radio.getPacketLength();

    if (checkPacketValidity(buffer, actualLength)) {
      logEvent("Packet valid: ");
      handleRadioPacket(buffer, actualLength);
    } else {
      logEvent("Invalid LoRa packet");
    }

  } else if (recvState != RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.print("Receive error: ");
    Serial.println(recvState);
  }
}

// deals with radio commands
void handleRadioPacket(uint8_t* packet, int length) {
  printPacket(packet, length);

  if (packet[2] != DEVICE_ID) {   //if DEST_ID doesn't equal this device
    loraSerial.write(packet, length);
    return;
  }
}

void handleMegaInput() {
  static uint8_t buffer[64];
  static int index = 0;

  while (loraSerial.available()) {
    uint8_t byte = loraSerial.read();

    // Wait for header
    if (index == 0 && byte != HEADER) continue;

    buffer[index++] = byte;

    // Prevent overflow
    if (index >= 64) {
      index = 0;
      continue;
    }

    // Packet complete
    if (byte == FOOTER && index >= 8) {
      if (checkPacketValidity(buffer, index)) {
        handleMegaPacket(buffer, index);
      } else {
        logEvent("Invalid LoRa packet");
      }

      index = 0;   // reset for next packet
    }
  }
}

// handles any commands given over UART from the Mega
void handleMegaPacket(uint8_t* packet, int length) {
  // check if any command is applicable to this device, otherwise forward over radio
  if (packet[2] == DEVICE_ID) {   //if DEST_ID doesn't equal this device
    uint8_t command = packet[4];
    uint8_t fault = 0;

    switch (command) {
      default:
        break;
    }

    sendCommandAck(command, fault);   //0, no faults
  } else {
    sendRadioPacket(packet, length);
  }
}

// ---------------- TRANSMIT ----------------
// sends a packet to confirm which command was executed, and the error status
void sendCommandAck(uint8_t originalCommand, uint8_t faultRaised) {
  uint8_t packet[9];

  packet[0] = HEADER;
  packet[1] = VERSION; 
  packet[2] = 4;   //dest_id, mega_football 
  packet[3] = DEVICE_ID;
  packet[4] = 100;        // acknowledgement command
  packet[5] = originalCommand;
  packet[6] = faultRaised;
  packet[7] = computeCRC8(packet, 7);
  packet[8] = FOOTER;

  loraSerial.write(packet, 9);
  logEvent("Acknowledgement sent");
  printPacket(packet, 9);
}

// send a packet five times over radio
void sendRadioPacket(uint8_t* lastPacket, int length) {
  printPacket(lastPacket, length);

  for (int i = 0; i < 5; i++) {   // repeat five times, to ensure command is received
    radio.standby();

    int err = radio.transmit(lastPacket, length);
    if (err != RADIOLIB_ERR_NONE) {
      logEvent("Telemetry TX Fail");
    }
    delay(30);  // small gap so packets don't collide with each other
  }
  radio.startReceive();
}

// sends error codes to the Lora
void sendError(int errorCommand) {
  uint8_t packet[7];

  packet[0] = HEADER;
  packet[1] = VERSION;      // version
  packet[2] = 4;      // destination mega_football
  packet[3] = DEVICE_ID;      // source 
  packet[4] = errorCommand;
  packet[5] = computeCRC8(packet, 5);
  packet[6] = FOOTER;

  loraSerial.write(packet, 7);      //send immediately
}


// ========================================================
// ====================== SETUP ===========================
// ========================================================
void setup() {
  Serial.begin(115200);

  loraSerial.begin(115200, SERIAL_8N1, RX_LORA, TX_LORA);
  SPI.begin(9, 11, 10, NSS);

  if (radio.begin(900.0, 125.0, 10, 8, 0x12, 22) != RADIOLIB_ERR_NONE) {
    while (true);
  }

  logEvent("System Boot");
}

// ========================================================
// ======================= LOOP ===========================
// ========================================================
void loop() {     
  handleMegaInput();  
  handleRadioReceive();
}