#include <EMGFilters.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// -----------------------------------------------------------------------------
// SETTINGS
// -----------------------------------------------------------------------------
#define SENSOR_PIN       A1
#define BUTTON_PIN       A2
#define SAMPLE_RATE      SAMPLE_FREQ_2000HZ
#define NOTCH_FREQ       NOTCH_FREQ_60HZ

// Serial Plotter Decimation (Print every 8th sample to serial for monitoring)
#define PRINT_EVERY_N_SAMPLES  8 

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------
const int chipSelect = BUILTIN_SDCARD; // Use Teensy 4.1 onboard slot
uint32_t timeBudgetUs;
uint8_t printCounter = 0;
uint16_t flushCounter = 0;
unsigned long timeStampStart = 0;
EMGFilters myFilter;
static float NOISE_THRESHOLD = 175;

// SD Card Objects
File dataFile;
char fileName[32];
bool sdActive = false;

void setup() {
  Serial.begin(115200);

  // 1. ADC Setup (High Precision)
  analogReadResolution(12); // 0 to 4095
  analogReadAveraging(1);   // Fastest speed

  // 2. Timing Calculation
  timeBudgetUs = 1000000UL / SAMPLE_RATE;

  // 3. SD Card Setup
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present.");
    sdActive = false;
  } else {
    Serial.println("card initialized.");
    sdActive = true;

    // Find unused filename (EMG_RAW_00.csv -> EMG_RAW_99.csv)
    for (uint8_t i = 0; i < 100; i++) {
      sprintf(fileName, "EMG_RAW_%02d.csv", i);
      if (!SD.exists(fileName)) {
        break;
      }
    }

    dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      Serial.print("Recording RAW data to: "); Serial.println(fileName);
      // Write Header
      dataFile.println("Time(us),Filtered");
      timeStampStart = micros();
    } else {
      Serial.println("Error opening file!");
      sdActive = false;
    }
    myFilter.init(SAMPLE_RATE, NOTCH_FREQ, true, true, true);
  }
  pinMode(BUTTON_PIN, INPUT);
}

void loop() {
  static elapsedMicros sampleTimer;
  
  // High-precision 2000 Hz loop
  if (sampleTimer >= timeBudgetUs) {
    sampleTimer -= timeBudgetUs;

    // 1. Capture Data
    
    float rawValue = (float)analogRead(SENSOR_PIN);
    float filteredValue = myFilter.update(rawValue);
    int buttonState = digitalRead(BUTTON_PIN)
    
    // timestamp
    unsigned long currentTime = micros()-timeStampStart;

    // 2. Log to SD Card
    if (sdActive) {
      dataFile.print(currentTime);
      dataFile.print(",");
      dataFile.println(filteredValue);
      dataFile.print(",");
      dataFile.println(filteredValue);

      // Flush data to physical card every ~1 second (2000 samples)
      // This prevents data loss if power is cut, without slowing down every loop
      flushCounter++;
      if (flushCounter >= 2000) {
        flushCounter = 0;
        dataFile.flush(); 
      }
    }



    // 3. Serial Monitor (Optional - just to verify it's working)
    // We print less often to save USB bandwidth
    printCounter++;
    if (printCounter >= PRINT_EVERY_N_SAMPLES) {
      printCounter = 0;
      Serial.print(0);          // Lower anchor
      Serial.print(",");
      Serial.print(10000);       // Upper anchor
      Serial.print(",");
      Serial.print(filteredValue);       // Upper anchor
      Serial.print(",");
      Serial.println(rawValue); // The raw signal
    }
  }
}