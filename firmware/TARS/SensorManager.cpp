#include "SensorManager.h"

#include <DFRobot_B_LUX_V30B.h>
#include <DFRobot_AirQualitySensor.h>

#include "ClosedCube_HDC1080.h"
#include "StateMachine.h"

extern ClosedCube_HDC1080 hdc1080;
extern DFRobot_B_LUX_V30B luxSensor;
extern DFRobot_AirQualitySensor particle;
extern StateMachine stateMachine;

void SensorManager::begin() {
  float initialLux = luxSensor.lightStrengthLux();
  if (initialLux < 0) initialLux = 0;
  for (int i = 0; i < LUX_HISTORY_SIZE; i++) {
    luxHistory[i] = initialLux;
  }
  Serial.println("[SensorManager] Buffer de lux inicializado");

  airQualityOk = particle.begin();
  if (!airQualityOk) {
    Serial.println("[SensorManager] Error sensor calidad de aire");
  } else {
    Serial.println("[SensorManager] Sensor calidad de aire OK");
  }
}

static float medianOf(float arr[], int size) {
  float sorted[15];
  memcpy(sorted, arr, size * sizeof(float));
  for (int i = 1; i < size; i++) {
    float key = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  return sorted[size / 2];
}

void SensorManager::read() {
  // ===== TEMPERATURA Y HUMEDAD con validación de rango HDC1080 =====
  float rawTemp = hdc1080.readTemperature();
  float rawHum = hdc1080.readHumidity();

  if (rawTemp > HDC_TEMP_MIN && rawTemp < HDC_TEMP_MAX) {
    stateMachine.sensors.temp = rawTemp + TEMP_OFFSET;
    hdc1080ErrorCount = 0;
  } else {
    hdc1080ErrorCount++;
    Serial.printf("[SensorManager] HDC1080 temp fuera de rango (%.1f °C) — conservando último valor válido (errores consecutivos: %d)\n", rawTemp, hdc1080ErrorCount);
  }

  if (rawHum >= HDC_HUM_MIN && rawHum <= HDC_HUM_MAX) {
    stateMachine.sensors.hum = rawHum - HUM_OFFSET;
  } else {
    Serial.printf("[SensorManager] HDC1080 hum fuera de rango (%.1f %%) — conservando último valor válido\n", rawHum);
  }

  // ===== LUX con filtro de anomalías + filtro de salto =====
  float rawLux = luxSensor.lightStrengthLux();

  if (rawLux < 0 || rawLux > EXTREME_LUX_THRESHOLD) {
    stateMachine.sensors.lux = luxHistory[(luxHistoryIndex - 1 + LUX_HISTORY_SIZE) % LUX_HISTORY_SIZE];
    Serial.print("[SensorManager] Lux anómalo (");
    Serial.print(rawLux);
    Serial.println(") — usando último valor válido");
  } else {
    float calibratedLux = (rawLux - LUX_CALIBRATION_OFFSET) / LUX_CALIBRATION_SLOPE;
    if (calibratedLux < 0) calibratedLux = 0.0f;

    float medianLux = medianOf(luxHistory, LUX_HISTORY_SIZE);

    if (fabs(calibratedLux - medianLux) > LUX_MAX_JUMP) {
      // Salto imposible en 2s -> pico transitorio, se descarta y NO entra al buffer
      stateMachine.sensors.lux = medianLux;
      Serial.printf("[SensorManager] Lux pico descartado (%.1f vs mediana %.1f)\n", calibratedLux, medianLux);
    } else {
      stateMachine.sensors.lux = calibratedLux;
      luxHistory[luxHistoryIndex] = calibratedLux;
      luxHistoryIndex = (luxHistoryIndex + 1) % LUX_HISTORY_SIZE;
    }
  }

  // ===== RUIDO =====
  int rawADC = analogRead(SOUND_SENSOR_PIN);
  float voltage = rawADC * (VREF / 4096.0f);
  stateMachine.sensors.voltage = voltage;
  stateMachine.sensors.dbValue = voltage * 50.0f;

  // ===== CALIDAD DE AIRE =====
  if (airQualityOk) {
    stateMachine.sensors.pm1_0 = particle.gainParticleConcentration_ugm3(PARTICLE_PM1_0_STANDARD);
    stateMachine.sensors.pm2_5 = particle.gainParticleConcentration_ugm3(PARTICLE_PM2_5_STANDARD);
    stateMachine.sensors.pm10  = particle.gainParticleConcentration_ugm3(PARTICLE_PM10_STANDARD);
  }

  // ===== ACUMULACIÓN PARA PROMEDIO =====
  accTemp += stateMachine.sensors.temp;
  accHum += stateMachine.sensors.hum;
  accLux += stateMachine.sensors.lux;
  accNoise += stateMachine.sensors.dbValue;
  if (airQualityOk) {                          
    accPm1_0 += stateMachine.sensors.pm1_0;
    accPm2_5 += stateMachine.sensors.pm2_5;
    accPm10 += stateMachine.sensors.pm10;
  }
  sampleCount++;

  Serial.printf("Temp: %.1f C | Hum: %.1f %% | Lux: %.1f | Ruido: %.1f dBA | PM1.0: %u PM2.5: %u PM10: %u | Muestras: %d\n",
                stateMachine.sensors.temp, stateMachine.sensors.hum, stateMachine.sensors.lux,
                stateMachine.sensors.dbValue, stateMachine.sensors.pm1_0, stateMachine.sensors.pm2_5,
                stateMachine.sensors.pm10, sampleCount);
}

SensorData SensorManager::getAverages() {
  if (sampleCount == 0) {
    Serial.println("[SensorManager] WARN: sin muestras acumuladas, usando último valor conocido como fallback");
    return stateMachine.sensors;
  }

  SensorData avg;
  avg.temp = accTemp / sampleCount;
  avg.hum = accHum / sampleCount;
  avg.lux = accLux / sampleCount;
  avg.dbValue = accNoise / sampleCount;
  avg.pm1_0 = airQualityOk ? (accPm1_0 / sampleCount) : 0;   
  avg.pm2_5 = airQualityOk ? (accPm2_5 / sampleCount) : 0;   
  avg.pm10 = airQualityOk ? (accPm10 / sampleCount) : 0;     

  Serial.printf("[SensorManager] Promedio de %d muestras — Temp: %.1f | Hum: %.1f | Lux: %.1f | Ruido: %.1f | PM1.0: %u | PM2.5: %u | PM10: %u\n",
                sampleCount, avg.temp, avg.hum, avg.lux, avg.dbValue, avg.pm1_0, avg.pm2_5, avg.pm10);
  return avg;
}

void SensorManager::resetAccumulator() {
  accTemp = 0.0f;
  accHum = 0.0f;
  accLux = 0.0f;
  accNoise = 0.0f;
  accPm1_0 = 0.0f;
  accPm2_5 = 0.0f;
  accPm10 = 0.0f;
  sampleCount = 0;
  Serial.println("[SensorManager] Acumulador reseteado");
}

