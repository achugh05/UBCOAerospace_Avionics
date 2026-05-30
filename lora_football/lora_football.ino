/*
Rev P5 - added sendError, command 17
Rev P6 - added comments
*/

#include <SPI.h>
#include <RadioLib.h>
#include <SD.h>

//system parameters
const int NUM_SERVOS = 3;
const int NUM_PRESSURES = 3;
int valvePositions[NUM_SERVOS];
int pressures[NUM_PRESSURES];

HardwareSerial loraSerial(1);   //UART bus declaration


// ================== common - RADIO CONFIG ==================
#define NSS    8
#define DIO1   14
#define RESET  12
#define BUSY   13
SX1262 radio = new Module(NSS, DIO1, RESET, BUSY);

// ================== SYSTEM CONFIG ==================
#define SD_CS 5
File dataFile;

#define RX_LORA 19    // alternatively, use 48 and 47 - though it may not work on specific club devices
#define TX_LORA 20

// transmission constants
#define HEADER 0xAB
#define VERSION 1
uint8_t DEVICE_ID = 3;
#define FOOTER 0xEF

unsigned long lastLoggedEvent = 0;


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
// common - used to give an explanation for events in the log
void logEvent(String message) {
  if (dataFile) {
    dataFile.print(millis());
    dataFile.print(",");
    dataFile.println(message);
  }
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

// common - logs packets to SD card
void logPacket(uint8_t* packet, int length) {
  if (dataFile) {
    dataFile.print(millis());
    dataFile.print(",");
    for (int i=0; i < length; i++) {
      dataFile.print(packet[i]);
      dataFile.print(",");
    }
    dataFile.println();
    if (millis() - lastLoggedEvent >= 300) {    //only flush to log every 300ms to avoid delays
      lastLoggedEvent = millis();
      dataFile.flush();
    }
  }
  
  printPacket(packet, length);  //used for serial debugging
}

//---------------INITIALIZING------------------------------
// common - initializes SD module and creates a unique file
void initializeDatalogging() {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed! NO LOGGING.");
    sendError(208);   //if sd card doesn't initialize properly, an error will be sent
    return;
  }

  int fileIndex = 0;
  String filename;

  while (true) {      //must create a new file upon power-up
    filename = "DATA_" + String(fileIndex) + ".csv";
    if (!SD.exists(filename)) break;
    fileIndex++;
  } 

  dataFile = SD.open(filename, FILE_WRITE);

  if (dataFile) {
    dataFile.print("Created file number ");
    dataFile.println(fileIndex);
  }
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
  logPacket(packet, length);

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
      case 17:    //reinitialize data card
        initializeDatalogging();
        break;

      case 207:   //query sd card status
        fault = (dataFile) ? 1 : 2;
        break;

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
  logPacket(packet, 9);
}

// send a packet five times over radio
void sendRadioPacket(uint8_t* lastPacket, int length) {
  logPacket(lastPacket, length);

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

// common - sends error codes to the Lora
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

// for manual commands and debugging, uncomment these functions
// void handleManualSerial() {   // meant only for debugging
//   if (!Serial.available()) return;

//   String s = Serial.readStringUntil('\n');
//   s.trim();

//   uint8_t packet[32];
//   int idx = 0;
//   int startIdx = 0;

//   while (startIdx < s.length() && idx < 32) {
//     int spaceIdx = s.indexOf(' ', startIdx);
//     String token;

//     if (spaceIdx != -1) {
//       token = s.substring(startIdx, spaceIdx);
//       startIdx = spaceIdx + 1;
//     } else {
//       token = s.substring(startIdx);
//       startIdx = s.length();
//     }

//     packet[idx++] = (uint8_t) token.toInt();
//   }

//   logEvent("Manual packet below:");
//   sendManualSerial(packet, idx);
// }

// // ---------------- PACKET BUILD ----------------
// void sendManualSerial(uint8_t* payload, int length) {
//   // length = number of bytes you entered, excluding CRC and footer
//   uint8_t packet[length + 2]; // +1 for CRC, +1 for FOOTER

//   // Copy payload
//   for (int i = 0; i < length; i++) {
//     packet[i] = payload[i];
//   }

//   // Compute CRC over payload
//   packet[length] = computeCRC8(packet, length); // CRC8 byte
//   packet[length + 1] = FOOTER;                  // Footer

//   int fullLength = length + 2; // full packet length including CRC and footer

//   // Check validity of full packet
//   if (checkPacketValidity(packet, fullLength)) {
//     sendRadioPacket(packet, fullLength);
//   } else {
//     logEvent("The manual packet below is invalid and not sent:");
//     logPacket(packet, fullLength);
//   }
// }

// ========================================================
// ====================== SETUP ===========================
// ========================================================
void setup() {
  Serial.begin(115200);

  loraSerial.begin(115200, SERIAL_8N1, RX_LORA, TX_LORA); // RX(19), TX(20)
  SPI.begin(9, 11, 10, NSS);

  if (radio.begin(900.0, 125.0, 10, 8, 0x12, 22) != RADIOLIB_ERR_NONE) {
    while (true);
  }

  initializeDatalogging();
  logEvent("System Boot");
}

// ========================================================
// ======================= LOOP ===========================
// ========================================================
void loop() {     
  handleMegaInput();  
  // handleManualSerial();    // for sending serial commands    // enter all fields except crc8 and footer
  handleRadioReceive();
}