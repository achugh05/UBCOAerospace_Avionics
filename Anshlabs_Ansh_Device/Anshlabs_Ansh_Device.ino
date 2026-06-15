//Board: Heltec LoRa Wifi ESP32 V3
//All devices are to be compatible with 5V
/* More comments and explanations
While the current ADC pins are untested, the code works for the listed alt pins
*/


#include <ADS1256.h>
#include <HX711.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>


//////////////////////////////////ADS/////////////////////////////
constexpr int8_t PIN_ADC0_SCK  = 42;    //alt pins include 19, 20, 48, 4
constexpr int8_t PIN_ADC0_MISO_DOUT = 40;
constexpr int8_t PIN_ADC0_MOSI_DIN = 41;
constexpr int8_t PIN_ADC0_DRDY = 39;
const uint8_t ADSchannels[3] = {1, 3, 5}; //must be ordered smallest to largest
const double ADS1256_VREF = 5.00;

SPIClass adcSpi(HSPI); // Instantiate the HSPI bus
ADS1256 adc0(          // Instantiate the ADS1256
  PIN_ADC0_DRDY,
  ADS1256::PIN_UNUSED,
  ADS1256::PIN_UNUSED,
  ADS1256::PIN_UNUSED,  //As it is the only device on the SPI bus, CS is hardwired to ground
                        //Because the pin is declared as unused, the library functions assume it to be grounded, which reduces delays from SPI confirmations
  ADS1256_VREF,
  &adcSpi
);
//////////////////////////////////ADS/////////////////////////////

//////////////////////////////////SD CARD/////////////////////////////
File dataFile;        //to log SD card to
constexpr int SD_SCK  = 35;
constexpr int SD_MISO = 34;
constexpr int SD_MOSI = 33;
constexpr int SD_CS   = 47;
SPIClass sdSPI(FSPI);
unsigned long lastLoggedEvent = 0;    //for use in determining if it has been a while since last flushed data to sd card
//////////////////////////////////SD CARD/////////////////////////////

//////////////////////////////////LOAD CELL/////////////////////////////
#define HX_DATA 6
#define HX_CLK  7
const double cali_value = 8.7;   //orginally 44.15 - update as required
HX711 cell;
double loadCellValue;   //globally update load cell value
//////////////////////////////////LOAD CELL/////////////////////////////

uint8_t pins[4] = {1, 3, 5, 38};  // built in ADC pins + sync voltage
double packet[8];
double pressure_caliA[3];   //pressure sensor calibration constants (calculated in calibratePressureSensors())
double pressure_caliB[3];




/////////////////////////////////FUNCTIONS///////////////////////////////
void initializeADC0() {
  logEvent("\n=================================");
  logEvent("    ANSHLABS™ ANSH DEVICE: ADS1256 TEST       ");
  logEvent("=================================");

  logEvent("[1/4] Starting HSPI Bus...");
  adcSpi.begin(PIN_ADC0_SCK, PIN_ADC0_MISO_DOUT, PIN_ADC0_MOSI_DIN);

  logEvent("[2/4] Initializing ADS1256...");
  adc0.InitializeADC();
  
  logEvent("[3/4] Configuring ADC Settings...");
  adc0.setDRATE(DRATE_30000SPS);    //set speed to max
  adc0.setPGA(PGA_1);
  adc0.setBuffer(1);

  logEvent("[4/4] Running Comms Health Check...");
  adc0.setMUX(SING_0);
  int32_t testVal = adc0.readSingle();

  // If MISO is disconnected, it usually returns 0 or 0xFFFFFFFF (-1)
  if (testVal == 0 || testVal == 0xFFFFFFFF) {
    logEvent(" ERROR: ADS1256 returned empty or faulty data.");
    logEvent("    -> Check your 5V power to the ADS1256.");
    logEvent("    -> Check MISO, MOSI, SCK, and CS wiring.");
    logEvent("    -> Check if pins 19/20 are crashing the USB.");
  } else {
    logEvent(" SUCCESS: ADS1256 is communicating perfectly!");
  }
}

void initializeDatalogging() {
  if (!sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS)) {
    Serial.println("SD init failed! NO LOGGING.");
  }

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
    Serial.println("There is no dataFile. Beware.");
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

// common - logs packets to SD card
void logPacket(double* packet, int length) {
  if (dataFile) {
    dataFile.print(millis());
    dataFile.print(",");
    for (int i=0; i < length; i++) {
      dataFile.print(packet[i]);
      dataFile.print(",");
    }
    dataFile.println();
    if (millis() - lastLoggedEvent >= 150) {    //avoid delays by not flushing every time
      lastLoggedEvent = millis();
      dataFile.flush();
    }
  }
  
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
  //V1, V2, P1, P2 values can be obtained experimentally by any two known pressure readings
  const double V1_mV[3] = {500, 500, 500};
  const double V2_mV[3] = {4500, 4500, 4500};
  const double P1[3] = {0, 0, 0};
  const double P2[3] = {500, 500, 500};

  for (int i=0; i<3; i++) {
    pressure_caliA[i] = (P2[i] - P1[i]) / (V2_mV[i] - V1_mV[i]);
    pressure_caliB[i] = P1[i] - pressure_caliA[i] * V1_mV[i];
  }
}

double convertMVtoPSI(double voltage, int cellNum) {
    //double psi = (voltage - volt2) * (psi1 / (psi2 * (volt1 - volt2)));
    return pressure_caliA[cellNum] * voltage / 1000 + pressure_caliB[cellNum];    //voltage is given in mV, needs to be in V
    //calibration constants calculated earlier in the code
}

void readADS1256() {
  // Read only desired channels
  int j = 0;
  for (int i = 0; i < 8; i++) {
    int32_t raw = adc0.cycleSingle();   //do not use undesired mux reading
    if (i == ADSchannels[j]) {
      j++;
      double voltage = ((double)raw / 8388607.0f) * ADS1256_VREF;
      packet[j] = convertMVtoPSI(voltage, j);
    }
  }
}

void readBuiltInADCs() {
  //built-in adc values (3 for pressure, 1 for sync)
  for (int i = 0; i < 3; i++) {   // log Pin values
    long raw = analogRead(pins[i]);   //config and read from channel i
    double voltage = ( (double)raw / 4095.0 ) * (3.3/1); // convert to voltage (vref=5, gain=1)  
    packet[i+3] = convertMVtoPSI(voltage, i);
  }
}

void readLoadCell() {
  if (cell.is_ready()) {
    loadCellValue = cell.get_units();   //only update with new data
  }
  packet[7] = loadCellValue;  //if no new value, remains old value (80Hz update)
}








//////////////////////////////////LOOP///////////////////////////////////
void setup() {
  Serial.begin(115200);
  initializeDatalogging();
  initializeLoadCell();
  initializeADC0();
  logEvent("Time,Channel0,Channel1,Channel2,Pin0,Pin1,Pin2,Sync,LoadCell");
}

void loop() {
  uint32_t t0 = micros();
  readADS1256();
  uint32_t t1 = micros();
  readBuiltInADCs();
  uint32_t t2 = micros();
  readLoadCell();
  uint32_t t3 = micros();
  logPacket(packet, 8);
  uint32_t t4 = micros();

  //analyze delays
  Serial.print("ADS1256: ");
  Serial.print(t1-t0);
  Serial.print(", Built In ADCs: ");
  Serial.print(t2-t1);
  Serial.print(", Load Cell: ");
  Serial.print(t3-t2);
  Serial.print(", Logging to SD Card: ");
  Serial.print(t4-t3);
  Serial.println();
}