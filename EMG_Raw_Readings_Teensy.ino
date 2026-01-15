#include <EMGFilters.h>
#include <Arduino.h>

// -----------------------------------------------------------------------------
// SETTINGS
// -----------------------------------------------------------------------------
#define SENSOR_PIN       A1
#define BUTTON_PIN       A2
#define SAMPLE_RATE      SAMPLE_FREQ_2000HZ
#define NOTCH_FREQ       NOTCH_FREQ_60HZ
#define RECOVERY_TIME_US 300000

// Serial Plotter Decimation
// 2000 Hz / 8 = 250 Hz visual update rate (Good for Serial Plotter)
#define PRINT_EVERY_N_SAMPLES  8 

// Threshold for noise gating (Adjust based on your noise floor)
// Signal below this magnitude will be zeroed out
static float NOISE_THRESHOLD = 173.205; 

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------
EMGFilters myFilter;
uint32_t timeBudgetUs;
uint8_t printCounter = 0;

void enterRecoveryMode();

void setup() {
  // 1. Serial Setup (Teensy USB Serial is always full speed, baud is ignored)
  Serial.begin(115200);
  
  // 2. ADC Setup for Teensy 4.1
  // 12-bit resolution = 0 to 4095
  analogReadResolution(12); 
  // No averaging inside hardware ensures we get the sharpest transients
  analogReadAveraging(1);   

  // 3. Filter Setup
  // Initialize with 2000Hz, 60Hz Notch, and all filters ON
  myFilter.init(SAMPLE_RATE, NOTCH_FREQ, true, true, true);

  // Calculate sample interval (2000Hz -> 500 microseconds)
  timeBudgetUs = 1000000UL / (uint32_t)SAMPLE_RATE;
  pinMode(BUTTON_PIN, INPUT);
}

void loop() {
  // 'static' keeps this variable alive between loop iterations
  // This prevents timing drift compared to creating it new every loop
  static elapsedMicros sampleTimer;
  static elapsedMicros recoveryTimer;
  

  if (sampleTimer >= timeBudgetUs) {
    // Subtract the budget to maintain precise average alignment
    // (e.g., if we are 2us late, next loop triggers 2us early to catch up)
    sampleTimer -= timeBudgetUs;


    float rawValue = (float)analogRead(SENSOR_PIN);

    if (rawValue >= 3730) {
      enterRecoveryMode();
      sampleTimer = 0;
      return;
    }

    float filteredValue = myFilter.update(rawValue);

    float processedEmg = 0;
    if (fabs(filteredValue) > NOISE_THRESHOLD) {
        // Squaring expands dynamics (large signals get much larger)
        processedEmg = filteredValue * filteredValue; 
    }

    printCounter++;
    if (printCounter >= PRINT_EVERY_N_SAMPLES) {
      printCounter = 0;

      // Format: Min, Max, Processed, Filtered, Raw
      // '0' and '4095' help lock the Serial Plotter vertical scale
      Serial.print(0);            // Lower bound anchor
      Serial.print(",");
      Serial.print(120000);         // Upper bound anchor (12-bit max)
      Serial.print(",");
      Serial.print(rawValue); // Blue Line: Squared Envelope
      Serial.print(",");
      Serial.print(filteredValue);
      Serial.print(",");
      Serial.print(processedEmg);
      Serial.print(","); // Red Line: Filtered (Offset to center)
      Serial.println();
    }
  }
}

void enterRecoveryMode() {
  myFilter.resetStates();

  elapsedMicros recoveryTimer = 0;
  elapsedMicros pacingTimer = 0;

  while (recoveryTimer < RECOVERY_TIME_US) {
    if (pacingTimer >= timeBudgetUs) {
      pacingTimer -= timeBudgetUs;
      float val = (float)analogRead(SENSOR_PIN);
      myFilter.update(val);
      Serial.print(0); Serial.print(",");
      Serial.print(120000); Serial.print(",");
      Serial.println(0);
    }
  }

}