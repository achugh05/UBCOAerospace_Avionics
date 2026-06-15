#include <SPI.h>
#include <SD.h>

const int SD_CS = 53;      // Mega default SS/CS pin
const char* TEST_FILE = "TEST.CSV";

void setup() {
  Serial.begin(115200);
  while (!Serial);  // Wait for Serial

  Serial.println(F("----- SD Card Debug for Mega -----"));

  // Ensure CS pin is HIGH before SD.begin() on Mega
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Initialize SPI & SD card
  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD initialization failed! Check wiring and CS pin."));
    while (true);
  }
  Serial.println(F("SD card detected."));

  // Check if file exists, create if not
  if (!SD.exists(TEST_FILE)) {
    Serial.print(F("File ")); Serial.print(TEST_FILE); Serial.println(F(" does not exist. Creating..."));
  } else {
    Serial.print(F("File ")); Serial.print(TEST_FILE); Serial.println(F(" already exists. Overwriting..."));
  }

  File f = SD.open(TEST_FILE, FILE_WRITE);
  if (!f) {
    Serial.println(F("Failed to open file for writing!"));
    while (true);
  }

  f.println("SD card debug test line.");
  f.close();
  Serial.println(F("Write successful."));

  // Read back file
  f = SD.open(TEST_FILE);
  if (!f) {
    Serial.println(F("Failed to open file for reading!"));
    while (true);
  }

  Serial.println(F("Reading back file contents:"));
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();
  Serial.println(F("\nRead complete. SD card working!"));
}

void loop() {
  // Nothing needed
}