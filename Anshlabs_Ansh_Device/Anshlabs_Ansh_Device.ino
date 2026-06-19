#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <HX711.h>
#include <ADS1256.h>

// --- PIN DEFINITIONS ---
constexpr int8_t PIN_ADC0_SCK  = 42;
constexpr int8_t PIN_ADC0_MISO_DOUT = 40; //labeled on ADS as DOUT
constexpr int8_t PIN_ADC0_MOSI_DIN = 41;  //labeled on ADS as DIN
constexpr int8_t PIN_ADC0_DRDY = 39;

constexpr float_t ADS1256_VREF = 5.00;

// built in ADC pins
uint8_t pins[4] = {1, 3, 5, 38};   //last pin is for sync voltage

File dataFile;        //to log SD card to
constexpr int SD_SCK  = 35;
constexpr int SD_MISO = 34;
constexpr int SD_MOSI = 33;
constexpr int SD_CS   = 47;
SPIClass sdSPI(FSPI);
unsigned long lastLoggedEvent = 0;    //for use in determining if it has been a while since last flushed data to sd card

// load cell
#define HX_DATA 7
#define HX_CLK  6
const float cali_value = 8.7;   //update as required
HX711 cell;
double loadCellValue;   //globally update load cell value

//pressure sensor calibration constants (calculated in calibratePressureSensors())
float pressure_caliA[3];
float pressure_caliB[3];


// Instantiate the HSPI bus
SPIClass adcSpi(HSPI);

// Instantiate the ADS1256
ADS1256 adc0(
  PIN_ADC0_DRDY,
  ADS1256::PIN_UNUSED,
  ADS1256::PIN_UNUSED,
  ADS1256::PIN_UNUSED,  //As it is the only device on the SPI bus, CS is hardwired to ground
                        //Because the pin is declared as unused, the library functions assume it to be grounded, which reduces delays from SPI confirmations
  ADS1256_VREF,
  &adcSpi
);


void initializeADC0() {
  logEvent("\n=================================");
  logEvent("    ANSHLABS™ ANSH DEVICE       ");
  logEvent("=================================");

  adcSpi.begin(PIN_ADC0_SCK, PIN_ADC0_MISO_DOUT, PIN_ADC0_MOSI_DIN);
  adc0.InitializeADC();
  adc0.setDRATE(DRATE_30000SPS);    //samples per second
  adc0.setPGA(PGA_1);               //gain
  adc0.setBuffer(1);

  //ensure starts on the expected channel
  adc0.setMUX(SING_0);
  int32_t testVal = adc0.readSingle();
}

void initializeDatalogging() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Start SPI with the working pins
  if (!SD.begin(SD_CS, sdSPI, 400000)) {
    logEvent("SD initialization failed!");
    while (true) {
      delay(1000);
    }
  }

  logEvent("SD initialization successful!");

  int fileIndex = 0;
  String filename;

  while (true) {      //must create a new file upon power-up
    filename = "/DATA_" + String(fileIndex) + ".csv";
    if (!SD.exists(filename)) {
      break;
    }
    fileIndex++;
  } 

  dataFile = SD.open(filename, FILE_WRITE);

  if (dataFile) {
    logEvent("Created file number " + String(fileIndex));
  } else {
    Serial.println("ERROR: NO DATAFILE - NO DATA IS BEING LOGGED");
  }
}

//prints packets to Serial monitor for use in debugging. 
void printPacket(double* packet, int telemetryLength) {
  for (int i=0; i<telemetryLength; i++) {
    Serial.print(packet[i]);
    Serial.print(" ");
  }
  Serial.println();   //function can be commented out to increase speed
}

// logs packets to SD card
void logPacket(double* packet, int length) {
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
  
  //comment out printPacket after calibration complete
  printPacket(packet, length);  //used for serial debugging   //comment out to increase speed
}

// used to give an explanation for events in the log
void logEvent(String message) {
  if (dataFile) {
    dataFile.print(millis());
    dataFile.print(",");
    dataFile.println(message);
  }
  Serial.println(message);    //for debugging only
}

void initializeLoadCell() {
  cell.begin(HX_DATA, HX_CLK);    //arduino pins
  cell.set_scale(cali_value);   //calibration values
  cell.tare();
  logEvent("Load cell set.");
}

void calibratePressureSensors() {
  //V1, V2, P1, P2 values obtained experimentally
  const float V1[3] = {0.5, 0.5, 0.5};
  const float V2[3] = {4.5, 4.5, 4.5};
  const float P1[3] = {0, 0, 0};
  const float P2[3] = {500, 500, 500};

  for (int i=0; i<3; i++) {
    pressure_caliA[i] = (P2[i] - P1[i]) / (V2[i] - V1[i]);
    pressure_caliB[i] = P1[i] - pressure_caliA[i] * V1[i];
  }
}

float convertVoltsToPSI(float voltage, int channel) {
  //y=Ax+B
  // Serial.print("volts-" + String(voltage) + " ");  //uncomment this line for help with calibration
  return pressure_caliA[channel] * voltage / + pressure_caliB[channel];
}

void setup() {
  Serial.begin(115200);
  
  if (!sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS)) {
    logEvent("SD init failed! NO LOGGING.");
  }
  initializeDatalogging();
  initializeLoadCell();
  initializeADC0();
  calibratePressureSensors();

  logEvent("Time,Channel0,Channel1,Channel2,Pin0,Pin1,Pin2,Sync,LoadCell");
}

void loop() {
  double packet[8];

  // Read ADS channels
  int j = 0;  //counter
  for (int i = 0; i < 8; i++) {
    int32_t raw = adc0.cycleSingle();
    if (i==1 || i==3 || i==5) {   //disregard values on channels we don't care about
      float voltage = ((float)raw / 8388607.0f) * ADS1256_VREF;

      packet[j] = convertVoltsToPSI(voltage, j);  //calibrated value per channel
      j++;
    }
  }

  //built-in adc values (3 for pressure, 1 for sync)
  for (int i = 0; i < 4; i++) {   // log Pin values
    long raw = analogRead(pins[i]);   //config and read from channel i
    float voltage = ( (float)raw / 4095.0 ) * (3.3/1); // convert to voltage (vref=5, gain=1)  
    packet[i+3] = voltage;  //built-in ADC's log pure voltage as they are a reference only and not calibrated
  }

  //load cell values
  if (cell.is_ready()) {
    loadCellValue = cell.get_units();   //only update with new data
  }
  packet[7] = loadCellValue;  //if no new value, remains old value (80Hz)
  logPacket(packet, 8);
  // delay(400);    //uncomment to view packets with a reasonable speed on serial monitor
}