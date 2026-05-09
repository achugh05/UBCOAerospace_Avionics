/* There appear to be no issues if a servo is at position "255 255". Encoder can track direction, although we cannot yet send controls for angles "below" zero.

Rev P1 - Board: Mega 2560. Mega + Heltec UART LoRa, 2-byte servo degree commands, full TX/RX logging to SD
Rev P2 - Added support for communication with LoRa, executing commands, logging initial errors. Servo code kept simple.
Rev P3 - Sequence removed. Added dest_id check. Add datalogging error control
Rev P4 - Added VERSION var. Fixed loraSerial to uses RX/TX_ARDUINO pins.
Rev P5 - checkPacketValidity doesn't need to check DEVICE_ID
- corrected CRC8 compute in sendError()
- removed duplicate prints, relies on logPacket() now
- updated command 9, deleted 10 and 11 (see AV-104)
- command ack addressed to mega_football, not lora
Rev P6 - Added support for pressure load cells, logs for five seconds after ignition
Rev P7 - fixed valve light update functions
- removed unused transmission errors
- added command 17
Rev P8 - added comments, reorganized some functions, updated print statements
- removed flush from logEvent()
- removed psi > 1600 limit
- added timestamp to logLoadCell()
- integrated buildPacket into sendPacket (now sendTelemetryPacket)
*/

#include <Encoder.h>
#include <SPI.h>
#include <SD.h>

// ---------------- SYSTEM CONFIG ----------------
#define NUM_SERVOS 3
#define NUM_PRESSURES 3
const int telemetryLength = 5 + NUM_SERVOS * 2 + NUM_PRESSURES * 2 + 2;

#define ID_PIN 22    //if pin high, manifold 1. if low, manifold 2
#define SD_CS 53    //for sd card
File dataFile;

// transmission constants
#define HEADER 0xAB
#define VERSION 1
uint8_t DEVICE_ID;
#define FOOTER 0xEF

HardwareSerial& loraSerial = Serial1;   //RX1 (19), TX1 (18)


unsigned long lastTelemetrySend = 0;
unsigned long lastLoggedEvent = 0;
long ignitionTime = 0;
const int burntime = 5000;

// ---------------- PRESSURE CALIBRATION ----------------
const int V1_mV = 500;
const int V2_mV = 2500;
const float P1 = -45;   //obtained experimentally
const float P2 = 800.0;

float A = (P2 - P1) / (float)(V2_mV - V1_mV);   //pressure sensor calibration
float B = P1 - A * V1_mV;

const int pressurePins[NUM_PRESSURES] = {A0, A1, A2};
uint16_t pressures[NUM_PRESSURES];
const int loadCellPressurePins[3] = {A4, A5, A6};

float pressure_caliA[NUM_PRESSURES] = {A, A, A};
float pressure_caliB[NUM_PRESSURES] = {B, B, B};
float loadcell_caliA[3] = {A, A, A};
float loadcell_caliB[3] = {B, B, B};




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
void logEvent(const char* message) {
  if (dataFile) {
    dataFile.print(millis());
    dataFile.print(",");
    dataFile.println(message);
  }
  //Serial.println(message);    //for debugging only
}

// common - prints packets to Serial monitor for use in debugging. 
// void printPacket(uint8_t* packet, int telemetryLength) {
//   for (int i=0; i<telemetryLength; i++) {
//     Serial.print(packet[i]);
//     Serial.print(" ");
//   }
//   Serial.println();
// }

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
  
  // printPacket(packet, length);  //used for serial debugging
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

void deviceConfig() {   //determines device ID, i.e. which manifold
  pinMode(ID_PIN, INPUT_PULLUP);
  DEVICE_ID =  (digitalRead(ID_PIN) == 1) ? 1 : 2;    //if pin high, manifold 1, if low, manifold 2
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

// ---------------- SERVO CLASS ----------------
class RexServo {
public:
  Encoder encoder;
  int rpwmPin;
  int lpwmPin;

  long targetTicks = 0;

  static constexpr float TICKS_PER_DEG = 0.67;
  const int SPEED = 120;
  const long TOL = 1;   // tolerance for position

  long unsigned moveStartTime;
  bool moveActive = false;
  const int timeout = 800; // will raise critical flag if times out

  RexServo(int encA, int encB, int rpwm, int lpwm)
    : encoder(encA, encB), rpwmPin(rpwm), lpwmPin(lpwm) {}

  void begin() {
    pinMode(rpwmPin, OUTPUT);
    pinMode(lpwmPin, OUTPUT);
    encoder.write(0);   // zero at boot
    stop();
  }

  void stop() {
    analogWrite(rpwmPin, 0);
    analogWrite(lpwmPin, 0);
  }

  void setAngle(uint16_t deg) {
    targetTicks = (long)(deg * TICKS_PER_DEG);
    moveStartTime = millis();
    moveActive = true;
  }

  long getTicks() {
    return encoder.read();
  }

  float getAngle() {
    return encoder.read() / TICKS_PER_DEG;
  }

  void update() {
    long pos = encoder.read();
    long err = targetTicks - pos;

    if (abs(err) <= TOL) {
      stop();
      moveActive = false;
      return;
    }

    if (err > 0) {
      analogWrite(rpwmPin, SPEED);
      analogWrite(lpwmPin, 0);
    } else {
      analogWrite(rpwmPin, 0);
      analogWrite(lpwmPin, SPEED);
    }

    if (moveActive && millis() - moveStartTime > timeout && abs(err) > TOL) {
      stop();
      moveActive = false;
      logEvent("Servo timeout. Please consider the implications.");
      sendError(250);
    }
  }
};

RexServo servos[NUM_SERVOS] = {
  RexServo(26, 27, 8, 7),     // encoder A, B, PWM A, B
  RexServo(28, 29, 6, 5),
  RexServo(30, 31, 10, 9)
};

// ---------------- PRESSURE FUNCTIONS ----------------
void readPressure() {   // reads through the array of pressures sensors
  for (int i = 0; i < NUM_PRESSURES; i++) {
    int raw = analogRead(pressurePins[i]);      //gathers the raw reading (10 bit number)
    long mV = (raw * 5000L) / 1023;   // converts to mV level (1023 is for 10-bit Mega ADC)
    float psi = pressure_caliA[i] * mV + pressure_caliB[i];     // converts mV reading to psi based on calibration parameters

    if (psi < 0) psi = 0;     // if lower than 0, reads 0

    pressures[i] = (uint16_t)psi;
  }
}

// used only for static fire
// cycles through each pressure sensor being used for the load cell and logs the pressure for later thrust calculations
void logLoadCell() {
  dataFile.print(millis());   //timestamp
  dataFile.print(",327,");   // arbitrary command value for ease of data parsing
  for (int i = 0; i < 3; i++) {
    int raw = analogRead(loadCellPressurePins[i]);
    long mV = (raw * 5000L) / 1023;   // 10-bit Mega ADC

    float psi = (float)loadcell_caliA[i] * mV + loadcell_caliB[i];

    dataFile.print(psi);
    dataFile.print(",");
  }
  dataFile.println();
}


// ---------------- RECEIVE ----------------
// parses UART input from the Lora
void handleLoraInput() {

  static uint8_t buffer[64];
  static int index = 0;

  while (loraSerial.available()) {

    uint8_t byte = loraSerial.read();

    // wait for header
    if (index == 0 && byte != HEADER)
      continue;

    buffer[index++] = byte;

    // if footer found, process packet
    if (index >= 2 && byte == FOOTER) {

      if (checkPacketValidity(buffer, index)) {
        handleLoraPacket(buffer, index);      // execute the command
      } else {
        logEvent("LoRa Packet Invalid");
      }

      index = 0;  // reset for next packet
    }

    if (index >= 64) {
      index = 0;  // overflow protection
    }
  }
}

// handles any commands given over UART from the Lora
void handleLoraPacket(uint8_t* packet, int length) {
  logEvent("Lora packet below:");
  logPacket(packet, length);
  uint8_t command = packet[4];

  if (packet[2] != DEVICE_ID) {   // if packet is meant for a different device, ignore
    logEvent("Packet above not meant for me. Maybe later.");
    return;
  }

  uint8_t fault = 0;

  switch (command) {
    uint16_t deg;

    case 4:   // this case should be removed for non-static fires. It is used as a redundant thrust logger
      logEvent("Ignition command. Beginning load cell pressures logging.");
      ignitionTime = millis();
      while (millis() - ignitionTime < burntime) { 
        logLoadCell();
        loop();   //untested, but it will make this nonblocking
      }
      break;

    case 9:   // change angle of servo
      int servoIndex = packet[5];
      deg = packet[6] << 8 | packet[7];    //degrees is sent in two bytes
      servos[servoIndex].setAngle(deg);
      break;

    case 13:    //close all servos
      for (int i = 0; i < NUM_SERVOS; i++) {
        servos[i].setAngle(0);
      }
      break;

    case 17:    //reinitialize data card
      initializeDatalogging();
      break;

    case 207:   //query sd card status
      fault = (dataFile) ? 1 : 2;
      break;

    default:
      logEvent("Invalid command.");
      break;
  }

  sendCommandAck(command, fault);   //0, no faults
}

// ---------------- TRANSMIT ----------------
// sends a packet to the Lora with updated telemetry
void sendTelemetryPacket() {
  uint8_t packet[telemetryLength];

  packet[0] = HEADER;
  packet[1] = VERSION;
  packet[2] = 4;      // destination mega_football
  packet[3] = DEVICE_ID;      // source
  packet[4] = 105;    // telemetry data command

  int idx = 5;
  for (int i = 0; i < NUM_SERVOS; i++) {
    uint16_t deg = (uint16_t)servos[i].getAngle();
    packet[idx++] = deg >> 8;         // separate servo valve positions into two bytes
    packet[idx++] = deg & 0xFF;
  }

  readPressure();
  for (int i = 0; i < NUM_PRESSURES; i++) {
    packet[idx++] = pressures[i] >> 8;      // separate pressures into two bytes
    packet[idx++] = pressures[i] & 0xFF;
  }

  uint8_t crc8 = computeCRC8(packet, idx);      // compute CRC of packet up to this point
  packet[idx++] = crc8;
  packet[idx++] = FOOTER;

  // relay to the Lora
  loraSerial.write(packet, telemetryLength);
  logPacket(packet, telemetryLength);
}

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


// ---------------- SETUP ----------------
void setup() {
  // Serial.begin(115200);    // used to start Serial output for debugging
  loraSerial.begin(115200);   // begins the UART channel to communicate with the Lora

  deviceConfig();             // determines if device is manifold 1 or 2

  for (int i = 0; i < NUM_SERVOS; i++)
    servos[i].begin();        // initializes PWM pins and sets each encoder to 0, 

  initializeDatalogging();    // initializes SD Card

  logEvent("System ready. Manifold ID: " + DEVICE_ID);
}

// ---------------- LOOP ----------------
void loop() {
  for (int i = 0; i < NUM_SERVOS; i++)
    servos[i].update();   // updates position of each servo valve

  handleLoraInput();    // checks for new UART data, executes commands

  if (millis() - lastTelemetrySend >= 100) {
    sendTelemetryPacket();    // builds and sends updated telemetry information
    lastTelemetrySend = millis();
  }
}