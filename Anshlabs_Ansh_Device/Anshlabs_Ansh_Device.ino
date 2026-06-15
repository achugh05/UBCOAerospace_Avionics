//Board: Heltec LoRa Wifi ESP32 V3
//All devices are to be compatible with 5V

/* FUTURE CHANGES TO INCLUDE */
/* More comments and explanations
  large operations extrapolated into functions
  Correcting serial writes to logEvent()
  Finalized testing of pinouts for ADS1256
  Remove timing statements
  Potentially further speed optimizations

  However, while the current ADC pins are untested, the code works for the listed alt pins
*/



#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <HX711.h>    //load cell amp library

// Prevent ADS1256 library from initializing with the default SPI instance
#define ADS1256_SPI_ALREADY_STARTED 1
#include <ADS1256.h>

// --- PIN DEFINITIONS ---
constexpr int8_t PIN_ADC0_SCK  = 42;    //alt pins include 19, 20, 48, 4
constexpr int8_t PIN_ADC0_MISO_DOUT = 40;
constexpr int8_t PIN_ADC0_MOSI_DIN = 41;
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
#define HX_DATA 6
#define HX_CLK  7
const float cali_value = 8.7;   //orginally 44.15 - update as required
HX711 cell;
double loadCellValue;   //globally update load cell value





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
  Serial.println("\n=================================");
  Serial.println("    ANSHLABS™ ANSH DEVICE: ADS1256 TEST       ");
  Serial.println("=================================");

  Serial.println("[1/4] Starting HSPI Bus...");
  adcSpi.begin(PIN_ADC0_SCK, PIN_ADC0_MISO, PIN_ADC0_MOSI);

  Serial.println("[2/4] Initializing ADS1256...");
  adc0.InitializeADC();
  
  Serial.println("[3/4] Configuring ADC Settings...");
  adc0.setDRATE(DRATE_30000SPS);
  adc0.setPGA(PGA_1);
  adc0.setBuffer(1);

  Serial.println("[4/4] Running Comms Health Check...");
  adc0.setMUX(SING_0);
  int32_t testVal = adc0.readSingle();

  // If MISO is disconnected, it usually returns 0 or 0xFFFFFFFF (-1)
  if (testVal == 0 || testVal == 0xFFFFFFFF) {
    Serial.println(" ERROR: ADS1256 returned empty or faulty data.");
    Serial.println("    -> Check your 5V power to the ADS1256.");
    Serial.println("    -> Check MISO, MOSI, SCK, and CS wiring.");
    Serial.println("    -> Check if pins 19/20 are crashing the USB.");
  } else {
    Serial.println(" SUCCESS: ADS1256 is communicating perfectly!");
  }

  // adc0.beginContinuous();
}

void initializeDatalogging() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Start SPI with the working pins
  if (!SD.begin(SD_CS, sdSPI, 400000)) {
    Serial.println("SD initialization failed!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("SD initialization successful!");

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

float convertVoltsToPSI(float voltage) {
    //float psi = (voltage - SENSOR_VMIN) * (PSI_MAX / (SENSOR_VMAX - SENSOR_VMIN));
    float psi = (voltage - 0.5) * (500 / (4.5 - 0.5));
    return psi;
}

void setup() {
  Serial.begin(115200);
  
  if (!sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS)) {
    Serial.println("SD init failed! NO LOGGING.");
  }
  initializeDatalogging();
  initializeLoadCell();
  initializeADC0();

  logEvent("Time,Channel0,Channel1,Channel2,Pin0,Pin1,Pin2,Sync,LoadCell");
}

void loop() {
  uint32_t t0 = millis();
  double packet[8];

  // Read channels
  for (int i = 0; i < 3; i++) {
    int32_t raw = adc0.cycleSingle();
    float voltage = ((float)raw / 8388607.0f) * ADS1256_VREF;
    packet[i] = convertVoltsToPSI(voltage);
  }
  for (int i=3; i<8; i++) {
    adc0.cycleSingle();
  }
  uint32_t t1 = millis();

  //built-in adc values (3 for pressure, 1 for sync)
  for (int i = 0; i < 4; i++) {   // log Pin values
    long raw = analogRead(pins[i]);   //config and read from channel i
    float voltage = ( (float)raw / 4095.0 ) * (3.3/1); // convert to voltage (vref=5, gain=1)  
    packet[i+3] = convertVoltsToPSI(voltage);
  }
  uint32_t t2 = millis();

  //load cell values
  if (cell.is_ready()) {
    loadCellValue = cell.get_units();   //only update with new data
  }
  packet[7] = loadCellValue;  //if no new value, remains old value (80Hz)
  uint32_t t3 = millis();
  logPacket(packet, 8);
  uint32_t t4 = millis();

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