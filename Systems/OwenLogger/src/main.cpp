/**
 * Code for Owen's Data Logger V2
 *
 * Specifically uses the QYF-0722V2 breakout board for the ADC1256 ADC,
 * the Adafruit NAU7802 ADC breakout board, a microSD card breakout board,
 * and an ESP32-WROOM-32 DEVKITV1 as the microcontroller. 
 * 
 * Improves on version 1 by taking advantage of the ESP32's dual cores to
 * perform asynchronous conversions and SD card writes, allowing for faster
 * sampling rates.
 * 
 * All code is kept within a single file to allow for use with the Arduino IDE.
 * 
 * Author: Julian Joaquin
 * Date: 2026-Apr-08
 * Version: 2.1.1
 * 
 * ADC0: ADS1256
 * ADC1: NAU7802
 * 
 * CHANGELOG:
 * 2.0.0: Release
 * 2.0.1: Added assertions for improved testing
 * 2.1.0: Refactored to make it easier for contributors to modify
 * 2.1.1: Older data files no longer get overwritten on SD card initialization
 * 2.1.2: Improved ADC1 timings
 * 
 * 
 *******************************************************************************
 * BINARY OUTPUT FILE STRUCTURE
 *******************************************************************************
 * 
 *  8-BITS -- NUMBER OF 32-BIT VALUES IN RECORD
 *  8-BITS -- RECORD SENSOR TYPE
 * 48-BITS -- RECORD TIME IN MICROSECONDS
 * 32-BITS -- RAW VALUE 0
 * 32-BITS -- RAW VALUE 1
 * ...     -- ...
 * (no record termination)
 * 
 *******************************************************************************
 * DEPENDENCIES
 *******************************************************************************
 * 
 * **BOARDS**
 * esp32                by Espressif Systems
 *
 * **LIBRARIES**
 * Adafruit NAU7802     by Adafruit
 * ADS1256              by Curious Scientist
 */


/*******************************************************************************
 * LOGGER INCLUDES (INTERNAL ONLY, DO NOT MODIFY)
 ******************************************************************************/

#include <Arduino.h>

#include <atomic>
#include <esp_system.h>
#include <esp_timer.h>
#include <SPI.h>
#include <SD.h>

// Production code will use the ESP32-WROOM-32 DEVKITV1
#if !defined(CONFIG_IDF_TARGET_ESP32)
  #error "This code is designed to run only on the ESP32-WROOM-32 DEVKITV1"
#endif

/*******************************************************************************
 * USER INCLUDES
 ******************************************************************************/

// Prevent ADS1256 library from initializing with the default SPI instance
#define ADS1256_SPI_ALREADY_STARTED 1

#include <Wire.h>
#include <Adafruit_NAU7802.h>
#include <ADS1256.h>

/*******************************************************************************
 * USER DEFINES
 ******************************************************************************/

//#define USE_METADATA_FILE
#define ENABLE_LOGGER_ASSERTS

/*******************************************************************************
 * LOGGER MACROS (INTERNAL ONLY, DO NOT MODIFY)
 ******************************************************************************/

#ifdef ENABLE_LOGGER_ASSERTS
  /* Invariant assertion checking. Enabled with `ENABLE_LOGGER_ASSERTS`. */
  #define LOGGER_ASSERT(cond, msg)                                         \
    do {                                                                   \
      if (!(cond)) {                                                       \
        char _assert_buf[256];                                             \
        snprintf(_assert_buf, sizeof(_assert_buf),                         \
                "ASSERT on [%s:%d]: \"%s\":\n\t%s",                        \
                 __FILE__, __LINE__, (msg), #cond);                        \
        faultHandler(_assert_buf);                                         \
      }                                                                    \
    } while (0)
  #define LOGGER_WARN_IF(cond, msg)                                        \
    do {                                                                   \
      if (cond) {                                                          \
        warn((msg));                                                       \
      }                                                                    \
    } while (0)
#else
  #define LOGGER_ASSERT(cond, msg)    do { } while (0)
  #define LOGGER_WARN_IF(cond, msg)   do { } while (0)
#endif

/*******************************************************************************
 * LOGGER PIN ASSIGNMENTS (INTERNAL ONLY, DO NOT MODIFY)
 ******************************************************************************/

#ifndef LED_BUILTIN
  // specific to the ESP32-DEVKITV1
  #define LED_BUILTIN 2
#endif
// SD card activity indicator
constexpr int8_t PIN_EXTLED1               = 13;
// fault indicator
constexpr int8_t PIN_EXTLED2               = 32;

// VSPI bus
constexpr int8_t PIN_ADC0_SPI_SCK          = 18;
constexpr int8_t PIN_ADC0_SPI_MISO         = 19;
constexpr int8_t PIN_ADC0_SPI_MOSI         = 23;
constexpr int8_t PIN_ADC0_SPI_CS           = 4;
constexpr int8_t PIN_ADC0_SPI_DRDY         = 34;

/*******************************************************************************
 * USER PIN ASSIGNMENTS
 ******************************************************************************/
// Just remember this is on a perfboard, and all pins must be soldered with
// wires to route to your intended ADC/sensor.

// alt SPI bus (I messed up the pins for HSPI but it's fine)
constexpr int8_t PIN_SD_SPI_SCK            = 33;
constexpr int8_t PIN_SD_SPI_MISO           = 25;
constexpr int8_t PIN_SD_SPI_MOSI           = 26;
constexpr int8_t PIN_SD_SPI_CS             = 15;

// I2C bus
constexpr int8_t PIN_ADC1_I2C_SDA          = 21;
constexpr int8_t PIN_ADC1_I2C_SCL          = 22;

/*******************************************************************************
 * LOGGER APPLICATION PROGRAMMING INTERFACE (DO NOT MODIFY)
 ******************************************************************************/

 /* Data Logger API */
namespace logger {
  /**
   * @brief Logger API - initial setup. Must be called at start of `setup()`.
   */
  void setupBegin();
  /**
   * @brief Logger API - ending setup. Must be called at end of `setup()`.
   */
  void setupEnd();
  /**
   * @brief Logger API - initial loop code. Must be called at start of `loop()`.
   */
  void loopBegin();
  /**
   * @brief Logger API - ending loop code. Must be called at end of `loop()`.
   */  
  void loopEnd();
  /**
   * @brief Logger API - Push data onto the SD card.
   * 
   * @param time_us The recorded time as an `unsigned long long`.
   * @param id      The numerical, user-defined ID of the sensor.
   * @param frame   The frame of 32-bit values.
   * @param len     The length of the frame in bytes.
   * @return true   when the recorded data is pushed to the SD card queue.
   * @return false  otherwise.
   * 
   * Use this function to record data from all ADCs/sensors.
   */
  bool pushRecord(uint64_t time_us, uint8_t id, const int32_t* frame,
                  size_t len);
  /**
   * @brief Logger API - Hard fault handler with a failure message over Serial.
   * 
   * @param user_msg The message indicating what has failed.
   * 
   * Use this function to hard-stop the data logger and force a system reset.
   */
  [[noreturn]] void faultHandler(const char* user_msg);
  /**
   * @brief Logger API - Send a warning message over Serial.
   * 
   * @param user_msg The warning message.
   * 
   * Use this function to signal the user of a minor problem.
   */
  void warn(const char* user_msg);
}

/*******************************************************************************
 * USER CONFIGURATION
 ******************************************************************************/

// VREF on QYF-0722V2 uses ADR03 2.5V voltage reference
constexpr float_t ADS1256_VREF   = 2.500;

constexpr uint8_t ADC0_DATA_ID   = 56;
constexpr size_t ADC0_FRAME_SIZE = 8;

constexpr uint8_t ADC1_DATA_ID   = 2;
constexpr size_t ADC1_FRAME_SIZE = 2;

// Flag to skip sampling ADC1
bool ADC1NotPresent = false;

SPIClass adcSpi(VSPI);

ADS1256 adc0(
  PIN_ADC0_SPI_DRDY,
  ADS1256::PIN_UNUSED,
  ADS1256::PIN_UNUSED,
  PIN_ADC0_SPI_CS,
  ADS1256_VREF,
  &adcSpi
);

Adafruit_NAU7802 adc1;

/* Add user sensor init functions here */

void initADC0() {
  constexpr uint8_t ADC0_SAMPLE_RATE = DRATE_30000SPS;
  constexpr uint8_t ADC0_GAIN = PGA_1;

  adc0.InitializeADC();
  adc0.setDRATE(ADC0_SAMPLE_RATE);
  adc0.setPGA(ADC0_GAIN);
}

void initADC1() {
  constexpr NAU7802_SampleRate ADC1_SAMPLE_RATE = NAU7802_RATE_320SPS;
  constexpr NAU7802_Gain ADC1_GAIN = NAU7802_GAIN_1;

  if (!adc1.begin(&Wire)) {
    logger::warn("ADC1 was not found! Continuing...");
    ADC1NotPresent = true;
    return;
  }
  if (!adc1.setRate(ADC1_SAMPLE_RATE)) {
    logger::warn("ADC1 could not adjust conversion rate. Continuing...");
  }
  if (!adc1.setGain(ADC1_GAIN)) {
    logger::warn("ADC1 could not adjust programmable gain. Continuing...");
  }

}

/* Add user sensor read functions here */
void readADC0() {
  // constexpr uint32_t ADC1_MINIMUM_PERIOD_US = 1850;

  int32_t frame[ADC0_FRAME_SIZE];
  // use `esp_timer_get_time()` instead of `micros()` so that timing can last
  // longer than 1.2 hours
  uint64_t timeNow_us = esp_timer_get_time();

  // Keep this loop as small as possible!
  for (int i = 0; i < 8; i++) {
    frame[i] = adc0.cycleSingle();
  }

  (void)logger::pushRecord(timeNow_us, ADC0_DATA_ID, frame, sizeof(frame));
}

void readADC1() {
  constexpr uint64_t ADC1_MINIMUM_PERIOD_US = 3700;
  static uint64_t lastSample_us = 0;

  int32_t frame[ADC1_FRAME_SIZE];
  uint64_t timeNow_us = esp_timer_get_time();

  // The NAU7802 takes a long time to transfer data over I2C. To improve data
  // throughput, we wait the equiavlent of 2 ADC0 reads before reading ADC1.
  if (timeNow_us < lastSample_us + ADC1_MINIMUM_PERIOD_US) {
    return;
  }

  if (!adc1.available()) {
    return;
  }
  if (!adc1.setChannel(0)) {
    return;
  }
  frame[0] = adc1.read();

  // Uncomment for Adafruit NAU7802 Rev. B
  // if (!adc1.setChannel(1)) {
  //   return;
  // }
  // frame[1] = adc1.read();

  (void)logger::pushRecord(timeNow_us, ADC1_DATA_ID, frame, sizeof(frame));

  lastSample_us = timeNow_us;
}

bool testADC0() {
  // Previous failure modes for the ADS1256 were caused by SPI faults resulting
  // in the received bits on MISO to be all 0 or 1. This typically materialized
  // as readings being either 0 or -1 (0xFFFFFFFF one's complement).
  // 
  // This test checks for the aforementioned failure mode by seeing if all
  // received values are just 0 or -1.

  int badTransmissionCount = 0;

  adc0.setMUX(SING_0);
  for (int i = 0; i < 10; i++) {
    int32_t conversion = adc0.readSingle();
    if ( (conversion == static_cast<int32_t>(0x00000000L))
      || (conversion == static_cast<int32_t>(0xFFFFFFFFL)) ) {
      
      badTransmissionCount++;
    }
    delay(10);
  }
  return badTransmissionCount < 9;
}

/*******************************************************************************
 * USER ARDUINO SKETCH
 ******************************************************************************/

void setup() {
  logger::setupBegin(); // keep here

  adcSpi.begin(PIN_ADC0_SPI_SCK, PIN_ADC0_SPI_MISO, PIN_ADC0_SPI_MOSI);
  if (!Wire.begin(PIN_ADC1_I2C_SDA, PIN_ADC1_I2C_SCL)) {
    logger::faultHandler("I2C initialization failed");
  }

  initADC0();
  initADC1();

  if (!testADC0()) {
    logger::faultHandler("ADC0 did not pass bus-fault test");
  }

  logger::setupEnd(); // keep here
}

void loop() {
  logger::loopBegin(); // keep here

  /* call your sensor read functions here */

  readADC0();
  
  if (!ADC1NotPresent) {
    readADC1();
  }

  logger::loopEnd(); // keep here
}

/*******************************************************************************
 *******************************************************************************
 * DATA LOGGER INTERNAL IMPLEMENTATION
 * DO NOT MODIFY CODE BELOW THIS LINE (UNLESS YOU KNOW WHAT YOU'RE DOING!)
 *******************************************************************************
 ******************************************************************************/

/**
 * # HELPFUL RESOURCES
 * 
 * SD SPI Host Driver documebtation
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_host.html 
 *
 * arduino-esp32 SD source code
 * https://github.com/espressif/arduino-esp32/tree/master/libraries/SD/src
 * 
 * ESP32 IDF FreeRTOS documentation
 * https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
 * 
 */

namespace logger {
namespace internal {

/*******************************************************************************
 * LOGGING CONSTANTS
 ******************************************************************************/

constexpr uint32_t MAX_LOOPS               = 65535; // arbitrarily short lol

constexpr uint32_t SERIAL_BAUD_RATE        = 115200;

constexpr size_t LOG_BUFFER_SIZE           = 8192; // this is in bytes!
constexpr size_t LOG_WRITE_THRESHOLD       = 4096; // this is in bytes!

constexpr uint8_t MAX_ROTATION_SKIPS       = 10; // arbitrary right now

static_assert(LOG_WRITE_THRESHOLD <= LOG_BUFFER_SIZE,
              "LOG_WRITE_THRESHOLD must be <= LOG_BUFFER_SIZE");

constexpr uint64_t SIZE_GIGABYTES = 1024 * 1024 * 1024;
constexpr uint64_t MIN_FREE_BYTES = 1 * SIZE_GIGABYTES;

constexpr size_t RECORD_HEADER_SIZE = sizeof(uint64_t);

constexpr char LOGFILE_NAME[] = "DATA_";
constexpr char LOGFILE_EXTENSION[] = ".bin";

constexpr char METAFILE_FILENAME[] = "metadata.txt";

/*******************************************************************************
 * TASK CONSTANTS
 ******************************************************************************/

constexpr BaseType_t SENSOR_TASK_CORE = 1;

constexpr uint32_t SD_CARD_TASK_STACK_SIZE = 8192; // this is in bytes!
constexpr BaseType_t SD_CARD_TASK_CORE = 0;
constexpr UBaseType_t SD_CARD_TASK_PRIORITY = tskIDLE_PRIORITY + 1;

/*******************************************************************************
 * TYPE DEFINITIONS
 ******************************************************************************/

enum class BufferState_t : uint8_t {
  BUFFER_FREE,
  BUFFER_FILLING,
  BUFFER_FULL,
  BUFFER_DRAINING
};

struct LogBuffer {
  uint8_t data[LOG_BUFFER_SIZE];
  size_t used;
};

/*******************************************************************************
 * VARIABLES
 ******************************************************************************/

// Flag for synchronized start with Core0
bool core0Ready = false;

// Signal to rotate the current active buffer in Core1
bool activeBufferFull = false;

// SPI buses are separate for two reasons:
// 1) The SD card breakout board controls the MISO pin even with CS pin high,
//    which causes interference with the ADC if they share an SPI bus.
// 2) Each core has its own SPI bus to avoid contention and allow for true
//    parallelism between ADC reads and SD card writes.
SPIClass sdSpi(HSPI);

// ESP32 SDFS library, not the Arduino SD library
fs::SDFS& sd = SD;
File logFile;
File metadataFile;

/*******************************************************************************
 * SHARED VARIABLES
 ******************************************************************************/

// buffer spinlock
portMUX_TYPE g_bufferMux = portMUX_INITIALIZER_UNLOCKED;

// indirectly shared variables
uint8_t g_fillIndex;
BufferState_t g_bufferState[2];
LogBuffer g_buffers[2];

// fault spinlock
portMUX_TYPE g_faultMux = portMUX_INITIALIZER_UNLOCKED;
// Directly accessed shared variables
std::atomic<bool> g_faulted(false);


TaskHandle_t g_sensorTaskHandle = nullptr;
TaskHandle_t g_sdCardHandle = nullptr;

// IPC message queue from Core1 to Core0
QueueHandle_t g_fullBufferQueue;

HardwareSerial& serial0 = Serial;

/*******************************************************************************
 * FUNCTION DECLARATIONS
 ******************************************************************************/
 
void sdWriterTask(void* pvParameters);

/* Heartbeat indication */
void blink();

uint64_t makeHeader(uint64_t time_us, uint8_t type, uint8_t length);
void faultGuard();
int32_t greatestFileIndex(const char* fNameStart, const char* fNameEnd,
                          File& rootFile);

/*******************************************************************************
 * CORE 0 FUNCTION DECLARATIONS
 ******************************************************************************/

bool initGPIO();
bool initSPI();

bool initSDCard();
bool initSDFiles();

bool drainFullBuffer(uint8_t bufferIndex);
bool writeBuffer(const uint8_t* buffer, size_t len);
bool writeMetadata();

/*******************************************************************************
 * CORE 1 FUNCTION DECLARATIONS
 ******************************************************************************/

bool rotateActiveBuffer();

} // namespace internal
} // namespace logging

/*******************************************************************************
 * API FUNCTIONS
 ******************************************************************************/

void logger::setupBegin() {
  using namespace internal;

  g_sensorTaskHandle = xTaskGetCurrentTaskHandle();

  // define initial buffer state
  g_fillIndex = 0;
  g_buffers[0].used = 0;
  g_buffers[1].used = 0;
  g_bufferState[0] = BufferState_t::BUFFER_FILLING;
  g_bufferState[1] = BufferState_t::BUFFER_FREE;

  // Serial must be initialized as early as possible
  serial0.begin(SERIAL_BAUD_RATE);

  LOGGER_ASSERT(xPortGetCoreID() == SENSOR_TASK_CORE,
                "Arduino loopTask is not using core 1");
}

void logger::setupEnd() {
  using namespace internal;

  g_fullBufferQueue = xQueueCreate(2, sizeof(uint8_t));
  if (g_fullBufferQueue == nullptr) {
    faultHandler("IPC Queue initialization failed");
  }

  LOGGER_ASSERT(xPortGetCoreID() != SD_CARD_TASK_CORE,
                "sdWriterTask will be assigned to same core as loopTask");

  BaseType_t xReturned = xTaskCreatePinnedToCore(
    sdWriterTask,
    "sdWriterTask",
    SD_CARD_TASK_STACK_SIZE,
    nullptr,
    SD_CARD_TASK_PRIORITY,
    &g_sdCardHandle,
    SD_CARD_TASK_CORE
  );
  if (xReturned != pdPASS) {
    faultHandler("Failed to create Core 0 task");
  }
}

void logger::loopBegin() {
  using namespace internal;

  if (!core0Ready) {
    // RTOS notification is used to block Core1 until Core0 initialization
    // is complete.
    //
    // Value of notification is meaningless in this context.
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    core0Ready = true;
  }

  faultGuard();
}

void logger::loopEnd() {
  using namespace internal;

  if (activeBufferFull) {
    activeBufferFull = !rotateActiveBuffer();
  }
}

bool logger::pushRecord(uint64_t time_us, uint8_t id, const int32_t* frame,
                        size_t len) {
  using namespace internal;

  const size_t recordSize = RECORD_HEADER_SIZE + len;

  portENTER_CRITICAL(&g_bufferMux);
  const uint8_t currentFillIndex = g_fillIndex;
  const BufferState_t fillState = g_bufferState[currentFillIndex];
  portEXIT_CRITICAL(&g_bufferMux);

  LOGGER_ASSERT(currentFillIndex == 0 || currentFillIndex == 1,
                "Core 0 received invalid buffer index");
  LOGGER_ASSERT(fillState == BufferState_t::BUFFER_FILLING,
                "Current filling buffer is not in valid state");

  // Prepare to rotate buffer if this write fills the buffer past threshold
  LogBuffer* buf = &g_buffers[currentFillIndex];
  activeBufferFull = (buf->used + recordSize >= LOG_WRITE_THRESHOLD);
  if (buf->used + recordSize > LOG_BUFFER_SIZE) {
    return false;
  }

  const uint8_t* byteFrame = reinterpret_cast<const uint8_t*>(frame);
  uint64_t header = makeHeader(time_us, id, len);
  for (size_t i = 0; i < RECORD_HEADER_SIZE; i++) {
    buf->data[buf->used++] = static_cast<uint8_t>((header >> (i*8)) & 0xFF);
  }
  for (size_t i = 0; i < len; i++) {
    buf->data[buf->used++] = byteFrame[i];
  }

  LOGGER_ASSERT(buf->used <= LOG_BUFFER_SIZE, "Buffer exceeded max size");
  return true;
}

[[noreturn]] void logger::faultHandler(const char* user_msg) {
  using namespace internal;

  LOGGER_ASSERT(user_msg != nullptr, "Fault message was a null pointer");

  portENTER_CRITICAL(&g_faultMux);
  bool first = !g_faulted;
  g_faulted = true;
  portEXIT_CRITICAL(&g_faultMux);
  
  // TODO: is there something better to do when we hit a fault?

  if (first) {
    (void)serial0.print("FAULT: ");
    (void)serial0.println(user_msg);
    serial0.flush();
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(PIN_EXTLED1, HIGH);
    digitalWrite(PIN_EXTLED2, LOW);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
  }
  else {
    while (true) {
      _NOP();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  while (true) {} // should never reach here
}

void logger::warn(const char* user_msg) {
  using namespace internal;

  LOGGER_ASSERT(user_msg != nullptr, "Warning message was a null pointer");
  (void)serial0.print("WARN: ");
  (void)serial0.println(user_msg);
  serial0.flush();
}

/*******************************************************************************
 * SD WRITER TASK DEFINITION
 ******************************************************************************/

void logger::internal::sdWriterTask(void* pvParameters) {

  (void)pvParameters; // unused parameter

  /* TASK SETUP */

  if (!initGPIO()) {
    faultHandler("Core 0 GPIO initialization failed");
  }
  if (!initSPI()) {
    faultHandler("Core 0 SPI initialization failed");
  }
  if (!initSDCard()) {
    faultHandler("Core 0 SD card initialization failed");
  }
  if (!initSDFiles()) {
    faultHandler("Core 0 SD card file initialization failed");
  }

  // Signal to Core1 that initialization is complete
  BaseType_t xReturned = xTaskNotifyGive(g_sensorTaskHandle);

  LOGGER_ASSERT(xReturned == pdPASS,
                "xTaskNotifyGive did not return expected value");

  /* TASK LOOP */
  while (true) {
    uint8_t bufferIndex = 0;

    faultGuard();

    // other LED indicators go here (SD card activity, errors, etc)
    blink();

    // block until buffer is filled by core1 and handed to core0
    if (xQueueReceive(g_fullBufferQueue, &bufferIndex, portMAX_DELAY)
        == pdTRUE) {
          
      LOGGER_ASSERT(g_fullBufferQueue != NULL,
                    "IPC message queue became NULL");
      LOGGER_ASSERT(bufferIndex == 0 || bufferIndex == 1,
                    "Core 0 received invalid buffer index");

      // drainFullBuffer handles errors internally
      (void)drainFullBuffer(bufferIndex);
    }
    
    //vTaskDelay(pdMS_TO_TICKS(1));
  }
}

/*******************************************************************************
 * INITIALIZATION FUNCTIONS 
 ******************************************************************************/

bool logger::internal::initGPIO() {
  /* Initialize GPIO pins */
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_EXTLED1, OUTPUT);
  pinMode(PIN_EXTLED2, OUTPUT);
  // pin `PIN_SD_SPI_CS` is controlled by the SD library

  /* Define initial states */
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(PIN_EXTLED1, LOW);
  digitalWrite(PIN_EXTLED2, HIGH);
  
  return true;
}

bool logger::internal::initSPI() {
  sdSpi.begin(PIN_SD_SPI_SCK, PIN_SD_SPI_MISO, PIN_SD_SPI_MOSI);
  return true;
}

bool logger::internal::initSDCard() {
  return sd.begin(PIN_SD_SPI_CS, sdSpi);
}

bool logger::internal::initSDFiles() {
  if (sd.totalBytes() - sd.usedBytes() < MIN_FREE_BYTES) {
    return false;
  }

  File root = sd.open("/");
  if (!root || !root.isDirectory()) {
    return false;
  }

  // Next file index after the highest existing DATA_x.bin
  const int nextIndex = 1 + greatestFileIndex(LOGFILE_NAME, LOGFILE_EXTENSION,
                                              root);

  // SDFS library requires absolute file path
  String filename = LOGFILE_NAME + String(nextIndex) + LOGFILE_EXTENSION;
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }

  LOGGER_ASSERT(!sd.exists(filename),
                "Attempting to create a file that already exists");

  logFile = sd.open(filename, FILE_WRITE, true);
  if (!logFile) {
    return false;
  }
  #ifdef USE_METADATA_FILE
    metadataFile = sd.open(METADATA_FILENAME, FILE_WRITE, false);
    return !!metadataFile;
  #else
    return true;
  #endif
}

/*******************************************************************************
 * CORE 0 FUNCTION DEFINITIONS
 ******************************************************************************/

bool logger::internal::drainFullBuffer(uint8_t bufferIndex) {
  
  portENTER_CRITICAL(&g_bufferMux);
  BufferState_t bufferState = g_bufferState[bufferIndex];

  LOGGER_ASSERT(bufferState == BufferState_t::BUFFER_FULL,
                "Buffer state not BUFFER_FULL after swap");

  g_bufferState[bufferIndex] = BufferState_t::BUFFER_DRAINING;
  portEXIT_CRITICAL(&g_bufferMux);

  LOGGER_ASSERT(g_bufferState[(bufferIndex+1)%2] != BufferState_t::BUFFER_DRAINING,
                "Both buffers are draining at the same time");

  LogBuffer* buf = &g_buffers[bufferIndex];

  LOGGER_ASSERT(buf->used <= LOG_BUFFER_SIZE, "Buffer exceeded max size");
  LOGGER_WARN_IF(buf->used == 0, "Core 0 Received empty buffer");
  
  if (!writeBuffer(buf->data, buf->used)) {
    faultHandler("SD card write transaction encountered a problem");
  }

  // DO NOT CHANGE THIS CODE!
  // This code section can only exist here. `BUFFER_FREE` must always
  // be assigned with `buf->used = 0`.
  portENTER_CRITICAL(&g_bufferMux);
  buf->used = 0;
  g_bufferState[bufferIndex] = BufferState_t::BUFFER_FREE;
  portEXIT_CRITICAL(&g_bufferMux);
  return true;
}

bool logger::internal::writeBuffer(const uint8_t* buffer, size_t len) {
  LOGGER_ASSERT(buffer != nullptr, "Passed buffer is a null pointer");
  LOGGER_ASSERT(len <= LOG_BUFFER_SIZE, "Passed buf length exceeds max size");

  digitalWrite(PIN_EXTLED2, HIGH);
  size_t written = logFile.write(buffer, len);
  if (written != len) {
    digitalWrite(PIN_EXTLED2, LOW);
    return false;
  }
  // Owen has emphasized the logger should save to non-volatile storage
  // as-soon-as-possible and as-fast-as-possible to mitigate data loss during
  // a catastrophic failure with the test stand. Testing had revealed the
  // ESP32 SDFS library is more than comfortable caching SD card `write()`
  // data and only flushing when the RAM is full, so `flush()` is manually
  // called at each `write()` to ensure data actually saves to the SD card.
  logFile.flush();
  digitalWrite(PIN_EXTLED2, LOW);

  return true;
}

bool logger::internal::writeMetadata() {
  // TODO: develop metadata format
  return true;
}

/*******************************************************************************
 * CORE 1 FUNCTION DEFINITIONS
 ******************************************************************************/

bool logger::internal::rotateActiveBuffer() {
  static uint8_t rotationSkips = 0;

  portENTER_CRITICAL(&g_bufferMux);
  uint8_t fullIndex = g_fillIndex;
  uint8_t nextIndex = (g_fillIndex + 1) % 2;

  if (g_bufferState[nextIndex] != BufferState_t::BUFFER_FREE) {
    // buffer is not free, skip this rotation
    rotationSkips++;
    portEXIT_CRITICAL(&g_bufferMux);
    if (rotationSkips > MAX_ROTATION_SKIPS) {
      faultHandler("Too many failed rotation tries");
    }
    return false;
  }
  rotationSkips = 0;

  LOGGER_ASSERT(g_bufferState[fullIndex] == BufferState_t::BUFFER_FILLING,
                "Current filling buffer is not in valid state");

  // rotate buffers
  g_bufferState[fullIndex] = BufferState_t::BUFFER_FULL;
  g_bufferState[nextIndex] = BufferState_t::BUFFER_FILLING;
  g_fillIndex = nextIndex; // rotate buffer index
  portEXIT_CRITICAL(&g_bufferMux);

  // signal core0 that a buffer is full and ready to write
  if (xQueueSend(g_fullBufferQueue, &fullIndex, 0) != pdTRUE) {
    faultHandler("xQueueSend failed to post full-buffer index during rotation due to full queue");
  }

  return true;
}

/*******************************************************************************
 * HELPER FUNCTION DEFINITIONS
 ******************************************************************************/

void logger::internal::blink() {
  static uint32_t lastLedOn = 0;
  static bool ledIsOn = false;

  constexpr static uint32_t PERIOD_MS = 4000;
  constexpr static uint32_t WIDTH_MS = 250;

  uint32_t now_ms = millis();

  if ((now_ms - lastLedOn >= PERIOD_MS) && !ledIsOn) {
    digitalWrite(LED_BUILTIN, HIGH);
    lastLedOn = now_ms;
    ledIsOn = true;
  }
  if ((now_ms - lastLedOn >= WIDTH_MS) && ledIsOn) {
    digitalWrite(LED_BUILTIN, LOW);
    ledIsOn = false;
  }
}

uint64_t logger::internal::makeHeader(uint64_t time_us, uint8_t type,
                                      uint8_t length) {
  uint64_t header = (time_us & 0x0000FFFFFFFFFFFFULL)
                  | (static_cast<uint64_t>(type) << 48)
                  | (static_cast<uint64_t>(length) << 56);
  // Header size guaranteed by function return type. 
  //LOGGER_ASSERT(sizeof(header) == RECORD_HEADER_SIZE,
  //              "Constructed header does not match specified header size");
  return header;
}

void logger::internal::faultGuard() {
  portENTER_CRITICAL(&g_faultMux);
  if (g_faulted) {
    portEXIT_CRITICAL(&g_faultMux);
    for (;;) {
      _NOP();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    // no return
  }
  portEXIT_CRITICAL(&g_faultMux);
}

int32_t logger::internal::greatestFileIndex(const char* fNameStart,
                                            const char* fNameEnd,
                                            File& rootFile) {
  int32_t highestIndex = -1;
  uint32_t loopCount = 0;

  while (loopCount < MAX_LOOPS) {
    loopCount++;
    File entry = rootFile.openNextFile();
    if (!entry) {
      break;  // no more files
    }

    // Skip directories
    if (entry.isDirectory()) {
      entry.close();
      continue;
    }

    String name = entry.name();
    entry.close();

    // Normalize in case the FS reports paths like "/DATA_3.bin"
    if (name.startsWith("/")) {
      name.remove(0, 1);
    }

    // Enforce file pattern
    if (!name.startsWith(fNameStart) || !name.endsWith(fNameEnd)) {
      continue;
    }

    // Extract the numeric part between prefix and suffix
    const int prefixLen = strlen(fNameStart);
    const int suffixLen = strlen(fNameEnd);
    const int numberLen = name.length() - prefixLen - suffixLen;

    if (numberLen <= 0) {
      continue;
    }

    String numberPart = name.substring(prefixLen, prefixLen + numberLen);

    // Ensure every character is a digit
    bool validNumber = true;
    for (int i = 0; i < numberPart.length(); i++) {
      if (!isDigit(numberPart[i])) {
        validNumber = false;
        break;
      }
    }

    if (!validNumber) {
      continue;
    }

    long idx = numberPart.toInt();
    if (idx < 0) {
      continue;
    }

    if (idx > highestIndex) {
      highestIndex = static_cast<int32_t>(idx);
    }
  }

  rootFile.close();

  LOGGER_WARN_IF(loopCount >= MAX_LOOPS,
                 "Exited loop due to exceeded max loop count");

  return highestIndex;
}

// "Many ethernet cables died to bring us this information"
