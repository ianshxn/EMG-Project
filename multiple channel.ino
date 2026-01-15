#include <EMGFilters.h>
#include <Arduino.h>

#define SENSOR_PIN_1            A0
#define SENSOR_PIN_2            A1
#define NOTCH_FREQ              NOTCH_FREQ_60HZ
#define SAMPLE_RATE             SAMPLE_FREQ_1000HZ
#define RECOVERY_TIME_US        300000
#define PRINT_EVERY_N_SAMPLES   8



static float NOISE_THRESHOLD = 173;

EMGFilters filter_1;
EMGFilters filter_2;
uint32_t timeBudgetUs;
uint8_t printCounter = 0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogReadAveraging(1);
  filter_1.init(SAMPLE_RATE, NOTCH_FREQ, true, true, true);
  filter_2.init(SAMPLE_RATE, NOTCH_FREQ, true, true, true);  
  timeBudgetUs = 1000000UL / (uint32_t)SAMPLE_RATE;
}

void loop() {
  static elapsedMicros sampleTimer;
  if (sampleTimer >= timeBudgetUs) {
    sampleTimer -= timeBudgetUs;
    float rawValue_1 = (float)analogRead(SENSOR_PIN_1);
    float rawValue_2 = (float)analogRead(SENSOR_PIN_2);
    float filteredValue_1 = filter_1.update(rawValue_1);
    float filteredValue_2 = filter_2.update(rawValue_2);
    printCounter++;
    if (printCounter >= PRINT_EVERY_N_SAMPLES) {
      printCounter = 0;
      Serial.print(0);            // Lower bound anchor
      Serial.print(",");
      Serial.print(5000);         // Upper bound anchor (12-bit max)
      Serial.print(",");
      Serial.print(filteredValue_1); // Blue Line: Squared Envelope
      Serial.print(",");
      Serial.print(filteredValue_2);
      Serial.println();
    }
  }
}
