/*
Note: if switches are armed and open on power-up, they will send the valve-open command
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
#define LED_PIN0 43
#define LED_PIN1 42
#define IGNITION_LED_PIN 44
#define ERROR_LED_PIN 45
#define ERROR_BUTTON1 47
// #define ERROR_BUTTON0 46
#define IGNITION_PIN 4
#define IGNITION_ARM_PIN 6
#define NUM_IGNITION_PIXELS 2
#define NUM_ERROR_PIXELS 3

const int switchPins[NUM_SWITCHES] = {29, 28, 31, 30, 25, 24, 27, 26};
Adafruit_NeoPixel leds[NUM_STRIPS] = {
  Adafruit_NeoPixel(NUM_PIXELS, LED_PIN0, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_PIXELS, LED_PIN1, NEO_GRB + NEO_KHZ800)
};
bool currentLedState[NUM_SWITCHES] = {false};
bool lastLedState[NUM_SWITCHES] = {false};

Adafruit_NeoPixel errorLEDs = Adafruit_NeoPixel(NUM_ERROR_PIXELS, ERROR_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ignitionLEDs = Adafruit_NeoPixel(NUM_IGNITION_PIXELS, IGNITION_LED_PIN, NEO_GRB + NEO_KHZ800);

// SD PINS (MEGA default SPI)
#define SD_CS 53
File dataFile;

HardwareSerial& loraSerial = Serial1;     //RX1 (19), TX1 (18)

uint8_t LORA_STATION = 0;
uint8_t MANIFOLD1 = 1;
uint8_t MANIFOLD2 = 2;

unsigned long lastLoggedEvent = 0;
unsigned long lastReceivedPacketTime;
const int connectivityTimeout = 9000;
bool connectivityError = false;
const int commandLength = 10;   //max command length (3 bytes of payload)
unsigned long lastCheck = 0;
int maxAcceptableDelay = 9000;                // self test - max time to wait for a command before flagging an error


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

bool selfTestAbort = false;   //to escape self-test mode

// pressure displays setup
const int displayDIOs[] = {10, 11, 9, 12};
#define displayCLK 13
TM1637Display display0(displayCLK, displayDIOs[0]);
TM1637Display display1(displayCLK, displayDIOs[1]);
TM1637Display display2(displayCLK, displayDIOs[2]);
TM1637Display display3(displayCLK, displayDIOs[3]);   //currently only required as a provision, and so is used for other things
TM1637Display displays[4] = {display0, display1, display2, display3};


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

// uncommon - logs packets to SD card
// also writes to mini PC for parsing
void logPacket(uint8_t* packet, int length) {
  // Serial.write(packet, length);   // for parsing by the mini PC
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
// initializes SD module and creates a unique file
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
    logEvent("Created file number " + String(fileIndex));
  } else {
      Serial.println("Issue with file.");
  }
}

// initializes pressure displays with a value of 9999
void initializeDisplays() {
  for (int i=0; i<NUM_PRESSURES; i++) {
    displays[i].setBrightness(3);   // scale of 1-7 (7 highest)
    displays[i].showNumberDec(9999, true);    // must use true to display
  }

  display3.setBrightness(3);   // provisional display. Used in some error code display behaviour
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
  // pinMode(ERROR_BUTTON0, INPUT_PULLUP);
  pinMode(ERROR_BUTTON1, INPUT_PULLUP);
  errorLEDs.begin();
  errorLEDs.clear();
  errorLEDs.show();

  // if error button is held down during power up, starts a self test
  // by only enabling the selfTest() function to be called during powerup, it cannot be accidentally run during normal operation
  if (digitalRead(ERROR_BUTTON1) == LOW) {     // check if a self test should be run
    delay(500);
    if (digitalRead(ERROR_BUTTON1) == LOW)   // ensure it wasn't an accident
      selfTest();     // execute self test
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
        sendCommandPacket(MANIFOLD1, 9, payload);    //manifold, command (move servo), payload
      }
    }
  }
}

// check ignition arm state, ignition button state. Sends the ignition command.
void checkIgnitionPanel() {
  // update arm status
  if (digitalRead(IGNITION_ARM_PIN) == LOW) {                            // if armed, turn red
    ignitionLEDs.setPixelColor(0, 255, 0, 0);   // red for armed
  } else {                                                  // if de-armed, turn off light
    ignitionLEDs.setPixelColor(0, 0, 0, 0);   // off
  }

  uint8_t current = digitalRead(IGNITION_PIN);    // read pin to determine if pressed
  if (current != lastIgnitionButtonState && (millis() - lastIgnitionButtonTime > DEBOUNCE_MS)) {    // debounce button
    lastIgnitionButtonTime = millis();      // update last time button was pressed
    lastIgnitionButtonState = current;      // store new state

    if (current == LOW) {                  // pressed
      sendCommandPacket(LORA_STATION, 4, &ignitionCode);    // send ignition command with ADDRESS of ignition code
      ignitionLEDs.setPixelColor(1, 255, 0, 0);   // ignition LED will be red until power off
    }
  }

  ignitionLEDs.show();                                        // show updates to the lights
}

// checks the error panel and updates the lights. One button/three lights
// One button is used as error reset
void checkErrorPanel() {
  uint8_t current = digitalRead(ERROR_BUTTON1);    // read error button pin to determine if pressed

  if (current != lastErrorButtonState && (millis() - lastErrorButtonTime > DEBOUNCE_MS)) {    // debounce button
    lastErrorButtonTime = millis();      // update last time button was pressed
    lastErrorButtonState = current;      // store new state

    if (current == LOW) {   // pressed
      for (int i=0; i < NUM_ERROR_PIXELS - 1; i++) {            // do not reset the connectivity light
        errorLEDs.setPixelColor(i, errorLEDs.Color(0, 0, 0));   // clear errors
      }
      logEvent("All errors cleared.");
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
  static uint8_t buffer[32];    // can test with 64, and just change the index reset as well
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
        if (connectivityError) {
          logEvent("Connection to Station has been recovered.");
          connectivityError = false;    // reset when a valid packet is received, NOT every time something is received
        }
        handleLoraPacket(buffer, index);
      } else {
        logEvent("LoRa Packet Invalid");
      }

      index = 0;  // reset for next packet
    }

    if (index >= 31) {
      index = 0;  // overflow protection
    }
  }

  // connectivity error, turn led red
  if (millis() - lastReceivedPacketTime > connectivityTimeout && !connectivityError) {   
    logEvent("ERROR - LOST CONNECTION TO STATION");
    errorLEDs.setPixelColor(2, errorLEDs.Color(100, 0, 0));
    errorLEDs.show();
    connectivityError = true;
    return;
  }
}

// parses UART input from the Lora
void handleLoraPacket(uint8_t* packet, int length) {
  lastReceivedPacketTime = millis();
  errorLEDs.setPixelColor(2, errorLEDs.Color(0, 255, 0));
  logPacket(packet, length);
  uint8_t command = packet[4];
  int idx = 5;

  if (packet[2] != DEVICE_ID) {   //if DEST_ID doesn't equal this device
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
        errorLEDs.setPixelColor(2, errorLEDs.Color(255, 255, 0));    //yellow
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
      if (packet[5] == 2) {
        display3.showNumberDec(packet[3]*1000 + 208);   //source_id + error code
        errorLEDs.setPixelColor(1, errorLEDs.Color(255, 255, 0));   //yellow
      }
      break;

    case 208:
      display3.showNumberDec(packet[3]*1000 + 208);   //source_id + error code
      errorLEDs.setPixelColor(1, errorLEDs.Color(255, 255, 0));   //yellow
      break;

    case 210:
      logEvent("ERROR - MEGA_STATION AND LORA_STATION CONNECTION LOST");
      errorLEDs.setPixelColor(2, errorLEDs.Color(0, 0, 255));   //blue
      break;

    case 250:
      display3.showNumberDec(9250);   //9 and servo timeout error
      errorLEDs.setPixelColor(1, errorLEDs.Color(255, 0, 0));   //red
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


// ---------------- SELF TEST FUNCTIONS ----------------
// prep: ensure all control switches are OFF and
// ensure Station is powered on and servos are in or near a closed position
/* SELF TEST DETAILS */
/*  -err2 light set yellow to indicate start
    -pressure displays show 4444
    -switch panel will flash a light green, then off, on each row, one at a time, for 500ms
    -a command is sent to each servo
*/
void selfTest() {
  logEvent("Beginning a self test.");         // log start
  errorLEDs.setPixelColor(0, errorLEDs.Color(255, 255, 0));   // yellow to indicate self test beginning
  errorLEDs.show();

  // shown numbers on the pressure displays
  for (int i=0; i<4; i++) {                   // for four displays
    displays[i].showNumberDec(4444);          // code 4444 should be visible on all displays throughout the self test
  }
  logEvent("Pressure displays showing 4444");
  delay(500);

  //flash the lights on (green) and off, one light in each row at a time
  leds[0].setPixelColor(0, leds[0].Color(0, 255, 0));     // turn only the first light green
  leds[1].setPixelColor(0, leds[1].Color(0, 255, 0));     // turn only the first light green
  leds[0].show();
  leds[1].show();
  delay(500);
  for (int i=1; i<NUM_PIXELS; i++) {
    leds[0].setPixelColor(i, leds[0].Color(0, 255, 0));   //green
    leds[0].setPixelColor(i-1, leds[0].Color(0, 0, 0));   //off
    leds[1].setPixelColor(i, leds[1].Color(0, 255, 0));   //green
    leds[1].setPixelColor(i-1, leds[1].Color(0, 0, 0));   //off
    leds[0].show();
    leds[1].show();
    delay(500);
  }
  leds[0].setPixelColor(NUM_PIXELS-1, leds[0].Color(0, 0, 0));   //off
  leds[1].setPixelColor(NUM_PIXELS-1, leds[1].Color(0, 0, 0));   //off
  leds[0].show();
  leds[1].show();

  int successfulServos = 0;   // used to track successful openings and closings
// opening each servo and waiting for acknowledgment, then closing
  for (int i=0; i<NUM_SERVOS; i++) {
    if (selfTestAbort) {
      logEvent("Self-test aborted.");
      return;
    }
    if (testMoveServo(i, 90))
      successfulServos++;
    if (testMoveServo(i, 0))
      successfulServos++;
  }

// set pressure displays to 0 for end of test
  for (int i=0; i<4; i++) {                   // for four displays
    displays[i].showNumberDec(0);             // set to zero
  }
  delay(1000);

// end self test mode
  logEvent("Self test mode ended.");
  logEvent(String("Amount of successful servo turns: ") + successfulServos);      // ratio of successful servo moves
  logEvent(String("Number of servos: ") + NUM_SERVOS);
  logEvent("Note that the successful servo turns should equal twice the number of servos.");
  errorLEDs.setPixelColor(0, errorLEDs.Color(0, 255, 0));   // green to indicate self test finished
  errorLEDs.show();
} // if there are no lights on the error panel other than light 2 as green, it is a success.
 
// sends command to move a servo to a specified angle
// waits the maxAcceptableDelay for acknowledgement or raises an error
bool testMoveServo(int servo, int angle) {
  if (selfTestAbort)
    return false;
  uint8_t payload[3] = {servo, 0, angle} ;   // servo, angle in two bytes (to ninety degrees)
  sendCommandPacket(MANIFOLD1, 9, payload);   // manifold, command (move servo), payload
  bool connectionTimedOut = true;     // initialize with true, change to false if successful contact made
  unsigned long testStartTime = millis();
  while (millis() - testStartTime < maxAcceptableDelay) {    // wait for confirmation to be received
    if (handleLoraInputSelfTest()) {     // note that the servo number is not returned in the command acknowledgement
      logEvent(String("Succesfully opened servo ") + servo);
      connectionTimedOut = false;
      break;
    }
    if (digitalRead(ERROR_BUTTON1) == LOW) {    //for abort
      delay(300);
      if (digitalRead(ERROR_BUTTON1) == LOW) {    //confirm abort
        selfTestAbort = true;             //set global variable
        return false;
      }
    }
    delay(10);
  }

  if (connectionTimedOut) {
    logEvent(String("WARNING: TIME OUT ERROR ON SERVO ") + servo);
    // add sending a command to the mini PC with the error code
    errorLEDs.setPixelColor(1, errorLEDs.Color(255, 0, 0));   //Light 1 to red for 250-servo timeout
    errorLEDs.show();
  }
  return !connectionTimedOut;   // return if servo was successful moved
}

// this is the handleLoraInput() code, with a different function call
bool handleLoraInputSelfTest() {
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

      if (checkPacketValidity(buffer, index)) {   // modified if statement
      // note that the servo number is not returned in the command acknowledgement
        // if command is acknowledgement of servo move command 9
        if (buffer[4] == 100 && buffer[5] == 9) {
          logPacket(buffer, index);
          if (buffer[6] != 0) {     // if there is a fault
            errorLEDs.setPixelColor(1, errorLEDs.Color(255, 255, 0));   //Light 1 to yellow as a warning
            errorLEDs.show();
            logEvent("Unknown servo fault detected above.");
          }
          index = 0;    // don't leave parser dirty, or next parsing may start corrupted
          return true;
        }
      }

      index = 0;  // reset for next packet
    }

    if (index >= 64) {
      index = 0;  // overflow protection
    }
  }
  return false;
}




// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);       // for print statements   // must be on for Serial.write to the mini PC
  loraSerial.begin(115200);   // begins the UART channel to communicate with the Lora

  initializeDisplays();       // initializes pressure displays
  initializeDatalogging();    // initializes SD Card
  initializeControlPanel();   // initializes valve control switches
  initializeIgnitionPanel();  // for ignition button
  initializeErrorPanel();     // for error lights and clearing button
      // error panel is last to initialize as it can enable a self test

  lastReceivedPacketTime = millis();      // to use in detecting connectivity errors
  logEvent("System ready. Device ID: " + String(DEVICE_ID));
  logEvent("Header,Version,Dest,Source,Command,Servo-0,Servo-0,Servo-1,Servo-1,Servo-2,Servo-2,Pres-0,Pres-0,Pres-1,Pres-1,Pres-2,Pres-2,CRC-8,Footer");
}

// ---------------- LOOP ----------------
void loop() {
  handleLoraInput();      // checks for new UART data, executes commands, updates telemetry displays

  if (millis() - lastCheck > 150) {
    checkControlPanel();            // check for new valve commands
    checkIgnitionPanel();           // check if ignition
    checkErrorPanel();              // check if errors cleared
    lastCheck = millis();
  }
}