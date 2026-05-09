/*
Rev P1 - Board: Mega 2560. deciphers telemetry and displays to TM1637 displays. control panel not complete yet
Rev P2 - Added compatibility with capstone. Added ability to make commands
- added control panel, capstone commands
- fixed rx-tx communication
- general fixes
Rev P3 - using commandLength seems wrong. should be a function of payload length
- fixed display and functioning of control panel, added servo turn commands
- datalogging error is displayed on error panel, not sent
Rev P4 - SD_CS corrected, updated display pins
- added ignition code, error lights
Rev P5 - added comments, updated print statements
- commands can send with variable length
- separated error panel, ignition panel functions
*/

const uint8_t ignitionCode = 27;
#include <SPI.h>
#include <SD.h>
#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>

// ---------------- SYSTEM CONFIG ----------------
#define NUM_SERVOS    3
#define NUM_PRESSURES 3

#define HEADER    0xAB
#define VERSION      1
#define DEVICE_ID    4
#define FOOTER    0xEF

#define NUM_PIXELS   4
#define NUM_SWITCHES 8
#define NUM_STRIPS   2
#define LED_PIN1 38
#define LED_PIN2 39
#define IGNITION_LED_PIN 42
#define ERROR_LED_PIN 41
#define ERROR_BUTTON_PIN 40
#define IGNITION_PIN 43
#define IGNITION_ARM_PIN 44
#define NUM_IGNITION_PIXELS 2
#define NUM_ERROR_PIXELS 3

const int switchPins[NUM_SWITCHES] = {37,35,33,31,29,27,25,23};
Adafruit_NeoPixel leds[NUM_STRIPS] = {
  Adafruit_NeoPixel(NUM_PIXELS, LED_PIN1, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_PIXELS, LED_PIN2, NEO_GRB + NEO_KHZ800)
};
bool currentLedState[NUM_SWITCHES] = {false};
bool lastLedState[NUM_SWITCHES] = {false};

Adafruit_NeoPixel errorLEDs = Adafruit_NeoPixel(NUM_IGNITION_PIXELS, IGNITION_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ignitionLEDs = Adafruit_NeoPixel(NUM_ERROR_PIXELS, ERROR_LED_PIN, NEO_GRB + NEO_KHZ800);

// SD PINS (MEGA default SPI)
#define SD_CS 53
File dataFile;

HardwareSerial& loraSerial = Serial1;     //RX1 (19), TX1 (18)

uint8_t LORA_STATION = 0;
uint8_t MANIFOLD1 = 1;
uint8_t MANIFOLD2 = 2;
uint8_t LORA_FOOTBALL = 3;

unsigned long lastLoggedEvent = 0;
unsigned long lastReceivedPacketTime;
const int connectivityTimeout = 3500;
bool connectivityError = false;
const int commandLength = 10;   //max command length (3 bytes of payload)
unsigned long lastCheckControlPanel = 0;

// arrays for states
uint16_t valvePositions[NUM_SERVOS];
int pressures[NUM_PRESSURES];

enum SwitchType { ARM, VALVE };
SwitchType switchType[NUM_SWITCHES] = {
    ARM, VALVE, ARM, VALVE,
    ARM, VALVE, ARM, VALVE
};

uint16_t valveDegrees[NUM_PIXELS];      // updated externally by mega_football

// button monitoring variables
const unsigned long DEBOUNCE_MS = 100;
uint8_t lastIgnitionButtonState = HIGH;
unsigned long lastIgnitionButtonTime = 0;
uint8_t lastErrorButtonState = HIGH;
unsigned long lastErrorButtonTime = 0;

unsigned long lastIgnitionArmState = 0;       // for ignition arming

// pressure displays setup
const int displayDIOs[] = {22, 24, 26, 28};
#define displayCLK 30
TM1637Display display0(displayCLK, displayDIOs[0]);
TM1637Display display1(displayCLK, displayDIOs[1]);
TM1637Display display2(displayCLK, displayDIOs[2]);
TM1637Display display3(displayCLK, displayDIOs[3]);   //currently only required as a provision, and so can be used for other things
TM1637Display displays[3] = {display0, display1, display2};


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
void printPacket(uint8_t* packet, int telemetryLength) {
  for (int i=0; i<telemetryLength; i++) {
    Serial.print(packet[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void logPacket(uint8_t* packet, int length) {
  Serial.write(packet, length);   // for parsing by the mini PC
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
// uncommon - initializes SD module and creates a unique file
void initializeDatalogging() {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed! NO LOGGING.");
    display3.showNumberDec(DEVICE_ID*1000 + 208);   //source_id + error code    //uncommon line
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

// initializes pressure displays with a value of 9999
void initializeDisplays() {
  for (int i=0; i<NUM_PRESSURES; i++) {
    displays[i].setBrightness(0x0f);
    displays[i].showNumberDec(9999, true);    // must use true to display
  }

  display3.setBrightness(0x0f);   //comment out if this display isn't to be active
  display3.showNumberDec(0, true);
}

// initializes switches for controlling servo valves
void initializeControlPanel() {
  for (int s = 0; s < NUM_STRIPS; s++) {
    leds[s].begin();
    leds[s].clear();
    leds[s].show();   // must use .show() to update colour
  }

  // ensure all pins are pullups by default
  for (int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(switchPins[i], INPUT_PULLUP);
  }
}

// initialize ignition arm and button, leds clear
void initializeIgnitionPanel() {
  pinMode(IGNITION_ARM_PIN, INPUT_PULLUP);
  pinMode(IGNITION_PIN, INPUT_PULLUP);
  ignitionLEDs.begin();
  ignitionLEDs.clear();
  ignitionLEDs.show();
}

// initialize ignition button, error leds clear
void initializeErrorPanel() {
  pinMode(ERROR_BUTTON_PIN, INPUT_PULLUP);
  errorLEDs.begin();
  errorLEDs.clear();
  errorLEDs.show();
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

// -------------------- CHECK FOR INPUTS --------------------
// update arm lights, check for new valve commands
void checkControlPanel() {
  // Read current switches state
  for (int i=0; i<NUM_SWITCHES; i++) {
    currentLedState[i] = (digitalRead(switchPins[i]) == LOW);
  }
  
  // update the arm leds
  for (int i=0; i<NUM_SWITCHES; i+=2) {
    int strip = i / 4;
    int pixel = i % 4;

    if (currentLedState[i]) {    //check if armed
      leds[strip].setPixelColor(pixel, leds[strip].Color(255, 0, 0));   //red
    } else {    //or de-arm
      leds[strip].setPixelColor(pixel, leds[strip].Color(0, 0, 0));     //off
    }
  }
  // show led changes 
  for (int s = 0; s < NUM_STRIPS; s++) {
    leds[s].show();
  }

  // see if any valve switches have changed
  for (int i=1; i<NUM_SWITCHES; i+=2) {
    if (lastLedState[i] != currentLedState[i]) {   //if different state, update
      lastLedState[i] = currentLedState[i];
      if (currentLedState[i-1]) {    //if armed, send command to move valve
        uint8_t payload[3] = {i/2, 0, currentLedState[i] ? 90 : 0} ;   //servo, angle (open or closed) in two bytes
        sendCommandPacket(1, 9, payload);    //manifold, command (move servo), payload
      }
    }
  }
}

// check ignition arm state, ignition button state. Sends the ignition command.
void checkIgnitionPanel() {
  uint8_t currentArmState = digitalRead(IGNITION_ARM_PIN);    // read arm switch state
  if (currentArmState!= lastIgnitionArmState) {               // check if arm state has changed
    lastIgnitionArmState = currentArmState;                   // update arm status
    if (currentArmState == LOW) {                            // if armed, turn red
      ignitionLEDs.setPixelColor(0, ignitionLEDs.Color(255, 0, 0));   // red for armed
    } else {                                                  // if de-armed, turn off light
      ignitionLEDs.setPixelColor(1, ignitionLEDs.Color(0, 0, 0));   // off
    }
  }

  uint8_t current = digitalRead(IGNITION_PIN);    // read pin to determine if pressed
  if (current != lastIgnitionButtonState && (millis() - lastIgnitionButtonTime > DEBOUNCE_MS)) {    // debounce button
    lastIgnitionButtonTime = millis();      // update last time button was pressed
    lastIgnitionButtonState = current;      // store new state

    if (current == LOW) {                  // pressed
      sendCommandPacket(0, 4, &ignitionCode);    // send ignition command with ADDRESS of ignition code
      ignitionLEDs.setPixelColor(1, ignitionLEDs.Color(255, 0, 0));   // red
    } else {
      ignitionLEDs.setPixelColor(1, ignitionLEDs.Color(0, 0, 0));     // off
    }
  }

  ignitionLEDs.show();                                        // show updates to the lights
}

// checks the error reset button and updates lights
void checkErrorPanel() {
  uint8_t current = digitalRead(ERROR_BUTTON_PIN);    // read error button pin to determine if pressed

  if (current != lastErrorButtonState && (millis() - lastErrorButtonTime > DEBOUNCE_MS)) {    // debounce button
    lastErrorButtonTime = millis();      // update last time button was pressed
    lastErrorButtonState = current;      // store new state

    if (current == LOW) {   // pressed
      for (int i=0; i < NUM_ERROR_PIXELS; i++) {
        errorLEDs.setPixelColor(i, errorLEDs.Color(255, 0, 0));   // red
      }
    }
  }

  errorLEDs.show();     // show led changes
}

// -------------------- Valve LED Logic --------------------
// update leds in accordance with new telemetry data
void updateValveLights(uint16_t* degrees) {
  int degreesIndex = 0;
  for (int i = 1; i < NUM_SERVOS*2; i += 2) {   // only update valve lights, not arms

    int strip = i / 4;
    int pixel = i % 4;
    int16_t mod = ((int16_t)degrees[degreesIndex++]) % 180;   //to deal with unsigned behaviour ("255 255" is -1)
    if (mod > 90) {
      mod -= 180;   //map to [-90, 90]
    }

    if (abs(mod) < 10) {
      leds[strip].setPixelColor(pixel, leds[strip].Color(255, 0, 0));   // red
    } else if (abs(mod) > 80) {
      leds[strip].setPixelColor(pixel, leds[strip].Color(0, 255, 0));   // green
    } else {
      leds[strip].setPixelColor(pixel, leds[strip].Color(0, 0, 255));   // blue
    }
  }

  // Update strips
  for (int s = 0; s < NUM_STRIPS; s++) {
    leds[s].show();
  }
}



// ---------------- RECEIVE ----------------
// handles any commands given over UART from the Lora
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
        handleLoraPacket(buffer, index);
      } else {
        logEvent("LoRa Packet Invalid");
      }

      index = 0;  // reset for next packet
    }

    if (index >= 64) {
      index = 0;  // overflow protection
    }
  }

  // connectivity error, turn led red
  if (millis() - lastReceivedPacketTime > connectivityTimeout && !connectivityError) {
    errorLEDs.setPixelColor(2, errorLEDs.Color(255, 0, 0));
    errorLEDs.show();
    connectivityError = true;
    return;
  }
}

// parses UART input from the Lora
void handleLoraPacket(uint8_t* packet, int length) {
  lastReceivedPacketTime = millis();
  errorLEDs.setPixelColor(2, errorLEDs.Color(0, 255, 0));
  logEvent("Lora packet below:");
  logPacket(packet, length);
  uint8_t command = packet[4];
  int idx = 5;

  if (packet[2] != DEVICE_ID) {   //if DEST_ID doesn't equal this manifold
    logEvent("Packet above not meant for me. Maybe later.");
    return;
  }
  
  switch (command) {

    case 100:
      if (packet[6] == 0) {
        logEvent("Acknowledge received.");
      } else if (packet[5] == 207 && packet[6] == 1) {
        logEvent("Ack. received. SD Card successfull.");
      } else if (packet[5] == 207 && packet[6] == 2) {
        logEvent("Ack. received. SD Card failed.");
        errorLEDs.setPixelColor(4, errorLEDs.Color(255, 0, 0));    //red
        display3.showNumberDec(packet[3]*1000 + 207);   //source_id + error code
      }
      break;

    case 105:   //parse telemetry data
      // Combine high + low bytes (big endian)
      for (int i=0; i<NUM_SERVOS; i++) {
        valvePositions[i] = ((uint16_t)packet[idx++] << 8) | packet[idx++];
      }
      updateValveLights(valvePositions);
      
      for (int i=0; i<NUM_PRESSURES; i++) {
        pressures[i] = ((uint16_t)packet[idx++] << 8) | packet[idx++];
        displays[i].showNumberDec(pressures[i]);
      }
      break;

    case 207:   //sd card query   //can't currently be sent to this device but code is here
      if (packet[5] == 2)
        display3.showNumberDec(packet[3]*1000 + 208);   //source_id + error code
        errorLEDs.setPixelColor(4, errorLEDs.Color(255, 0, 0));   //red
      break;

    case 208:
      display3.showNumberDec(packet[3]*1000 + 208);   //source_id + error code
      errorLEDs.setPixelColor(4, errorLEDs.Color(255, 0, 0));   //red
      break;

    case 250:
      display3.showNumberDec(3333);   //random servo code
      errorLEDs.setPixelColor(3, errorLEDs.Color(255, 0, 0));   //red
      break;

    default:
      logEvent("Invalid command.");
      break;
  }

  errorLEDs.show();
}

// ---------------- TRANSMIT ----------------
// builds and sends a new command packet to the Lora
void sendCommandPacket(uint8_t destination, uint8_t command, uint8_t* payload) {
  uint8_t packet[commandLength];

  // WARNING - IF ONLY ONE VALUE FOR PAYLOAD, MUST BE PASSED AS AN ADDRESS (&var)
  // Passing incorrect size of packet for the command case will cause unexpected behaviour
  packet[0] = HEADER;
  packet[1] = VERSION;      // version
  packet[2] = destination;
  packet[3] = DEVICE_ID;      // source 
  packet[4] = command;

  int idx = 5;
  int i = 0;

  switch (command) { 
    //FALL THROUGH IS INTENTIONAL BEHAVIOUR
    //sorted by how many bytes each command is expecting from payload

    // three byte payloads
    case 9:   //move servos
      packet[idx++] = payload[i++]; //intentional fall through

    // two byte payloads
    case 12:  //calibrate   //not yet supported
      packet[idx++] = payload[i++]; //intentional fall through

    // one byte payloads
    case 4:   //ignition code
      packet[idx++] = payload[i++]; //intentional fall through

    // no payloads
    case 207:   //sd card query
      break;

    default:
      //nothing
      break;
  }

  packet[idx] = computeCRC8(packet, idx);
  idx++;
  packet[idx++] = FOOTER;

  // send command to Lora
  loraSerial.write(packet, idx);    //write idx for variable length - not all commands are same length
  logPacket(packet, idx);
}





// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);       // for print statements
  loraSerial.begin(115200);   // begins the UART channel to communicate with the Lora

  initializeDisplays();       // initializes pressure displays
  initializeDatalogging();    // initializes SD Card
  initializeControlPanel();   // initializes valve control switches
  initializeIgnitionPanel();  // for ignition button
  initializeErrorPanel();     // for error lights and clearing button

  logEvent("System ready. Manifold ID: " + DEVICE_ID);
}

// ---------------- LOOP ----------------

void loop() {
  handleLoraInput();      // checks for new UART data, executes commands, updates telemetry displays

  if (millis() - lastCheckControlPanel > 150) {
    checkControlPanel();            // check for new valve commands
    checkIgnitionPanel();           // check if ignition
    checkErrorPanel();              // check if errors cleared
    lastCheckControlPanel = millis();
  }
}