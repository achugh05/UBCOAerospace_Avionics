const uint8_t ignitionCode = 27;

#include <RadioLib.h>

HardwareSerial loraSerial(1);

// ================== common - RADIO CONFIG ==================
#define NSS    8
#define DIO1   14
#define RESET  12
#define BUSY   13
SX1262 radio = new Module(NSS, DIO1, RESET, BUSY);

// ================== SYSTEM PINS ==================
#define IGNITION 4
#define RX_LORA 48
#define TX_LORA 47
#define RX_SECOND 19
#define TX_SECOND 20

// transmission constants
#define HEADER 0xAB
#define VERSION 1
uint8_t DEVICE_ID = 0;
#define FOOTER 0xEF

// ================== SYSTEM CONFIG ======================
#define MANIFOLD1   1
#define MANIFOLD2   2
#define CAPSTONE    6


uint8_t lastManifoldPacket[64];
int lastManifoldLength = 0;

unsigned long lastTelemetrySend = 0;
unsigned long lastReceivedPacketTime;
const int connectivityTimeout = 1200;
bool connectivityError = false;
uint16_t receivedIgnitionCode;

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
// common to loras - used to give an explanation for events in the log
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

void sendSyncSignal() {   //only for static fire or when other devices are being used
  const int SYNC = 19;
  pinMode(SYNC, OUTPUT);    //for syncing with other devices
  digitalWrite(SYNC, LOW);
  logEvent("Sync low");
  delay(300);
  digitalWrite(SYNC, HIGH);
  logEvent("Sync high");
  digitalWrite(SYNC, LOW);    // used for syncing time
  logEvent("Sync low");
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
  logEvent("Handling radio packet below: ");
  printPacket(packet, length);
  uint8_t command = packet[4];

  if (packet[2] != DEVICE_ID) {   //if DEST_ID doesn't equal this manifold, forward to manifolds
    loraSerial.write(packet, length);
    logEvent("Packet above not meant for me. Maybe later.");
    return;
  }

  uint8_t fault = 0;

  switch (command) {

    case 4: // Fire Ignition - Must be in brackets to prevent "jump to case label" error
      receivedIgnitionCode = packet[5];
      if (receivedIgnitionCode == ignitionCode) { 
        logEvent("Valid ignition sequence. Activating.");
        packet[2] = 1;    //forward to manifold 1, for a backup reading of the pressure sensor load cell. Remove the forwarding command if not used.
        loraSerial.write(packet, length);
        logEvent("Igniting. Sent command to Manifold 1 to log load cell pressures.");
        digitalWrite(IGNITION, HIGH);
        delay(1000); // Review required
        digitalWrite(IGNITION, LOW);
        logEvent("Ignition complete.");
      } else {
        logEvent("Invalid ignition code... beware");
      }
      break;

    default: break;
  }

  sendCommandAck(command, fault);   //0, no faults (faults not supported)
}

// handles any data given over UART from the manifold
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
      lastReceivedPacketTime = millis();
      if (checkPacketValidity(buffer, index)) {
        memcpy(lastManifoldPacket, buffer, index);
        lastManifoldLength = index;
        if (buffer[4] != 105) {   //immediately forward non-telemetry packets
          sendNextPacket();
        } else {
          printPacket(lastManifoldPacket, index);
        }
      }

      index = 0;   // reset for next packet
    }
  }
}

// ---------------- TRANSMIT ----------------
// sends a packet over radio
void sendRadioPacket(uint8_t* lastPacket, int length) {
  radio.standby();
  int err = radio.transmit(lastPacket, length);
  if (err != RADIOLIB_ERR_NONE) logEvent("Telemetry TX Fail");
  radio.startReceive();
  printPacket(lastPacket, length);
}

// sends most recent manifold packet
void sendNextPacket() {
  if (millis() - lastReceivedPacketTime > connectivityTimeout && !connectivityError) {
    sendError(209);
    connectivityError = true;   // if no contact recently with the manifold
    return;
  }
  connectivityError = false;
  sendRadioPacket(lastManifoldPacket, lastManifoldLength);
}

// sends error codes across the radio
void sendError(int error) {
  uint8_t packet[9];
  packet[0] = HEADER;
  packet[1] = VERSION;         
  packet[2] = 4;               // destination mega_football
  packet[3] = DEVICE_ID; 
  packet[4] = error;      
  packet[5] = computeCRC8(packet, 5);
  packet[6] = FOOTER;

  radio.standby();
  radio.transmit(packet, 7);
  radio.startReceive();
}

void sendCommandAck(uint8_t originalCommand, uint8_t fault) {
  uint8_t packet[9];

  packet[0] = HEADER;
  packet[1] = VERSION; 
  packet[2] = 4;   //dest_id, mega_football 
  packet[3] = DEVICE_ID;
  packet[4] = 100;        // acknowledgement command
  packet[5] = originalCommand;
  packet[6] = fault;
  packet[7] = computeCRC8(packet, 7);
  packet[8] = FOOTER;

  sendRadioPacket(packet, 9);
  logEvent("acknowledgement sent");
  printPacket(packet, 9);
}




// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // initialize ignition pin immediately
  pinMode(IGNITION, OUTPUT);
  digitalWrite(IGNITION, LOW);


  loraSerial.begin(115200, SERIAL_8N1, RX_LORA, TX_LORA); 
  SPI.begin(9, 11, 10, NSS);

  // begin radio
  if (radio.begin(900.0, 125.0, 10, 8, 0x12, 22) != RADIOLIB_ERR_NONE) {
    Serial.println("LoRa init failed!");
    while (true);
  }

  sendSyncSignal();   // for syncing other devices, raises a pin low-high-low

  logEvent("System Boot");
}

// ---------------- LOOP ----------------
void loop() {
  handleMegaInput();    // relays commands, stores telemetry

  if (millis() - lastTelemetrySend >= 400) {
    lastTelemetrySend = millis();
    sendNextPacket();   // sends most recent valid command over radio
  }
    
  handleRadioReceive(); // parses and relays radio commands to the manifold
}