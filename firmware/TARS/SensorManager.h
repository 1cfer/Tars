#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>

#include "StateMachine.h"

#define SOUND_SENSOR_PIN 35
#define VREF 3.7f

class SensorManager {
 private:
  static const int LUX_HISTORY_SIZE = 15;
  float luxHistory[LUX_HISTORY_SIZE];
  int luxHistoryIndex = 0;

  static constexpr float TEMP_OFFSET = -3.0f;
  static constexpr float HUM_OFFSET = 7.0f;
  static constexpr float LUX_CALIBRATION_OFFSET = 6.1551f;
  static constexpr float LUX_CALIBRATION_SLOPE = 1.3788f;
  static constexpr float EXTREME_LUX_THRESHOLD = 300000.0f;
  static constexpr float NOISE_CALIBRATION_SLOPE = 1.618f;
  static constexpr float NOISE_CALIBRATION_OFFSET = 14.282f;
  static constexpr float HDC_TEMP_MIN = -20.0f;
  static constexpr float HDC_TEMP_MAX = 85.0f;
  static constexpr float HDC_HUM_MIN = 0.0f;
  static constexpr float HDC_HUM_MAX = 100.0f;
  static constexpr float LUX_MAX_JUMP = 500.0f;  // salto máximo plausible entre lecturas de 2s

  int hdc1080ErrorCount = 0;
  bool airQualityOk = false;   // NUEVO: si el sensor de calidad de aire inicializó bien

  float accTemp = 0.0f;
  float accHum = 0.0f;
  float accLux = 0.0f;
  float accNoise = 0.0f;
  float accPm1_0 = 0.0f;
  float accPm2_5 = 0.0f;
  float accPm10 = 0.0f;
  int sampleCount = 0;

 public:
  void begin();
  void read();
  SensorData getAverages();
  void resetAccumulator();
  int getSampleCount() const { return sampleCount; }
  bool isAirQualityOk() const { return airQualityOk; } // NUEVO
};

extern SensorManager sensorManager;

#endif