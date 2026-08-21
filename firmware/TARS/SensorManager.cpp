#include "SensorManager.h"
#include <DFRobot_B_LUX_V30B.h>
#include "ClosedCube_HDC1080.h"
#include "StateMachine.h"

extern ClosedCube_HDC1080 hdc1080;
extern DFRobot_B_LUX_V30B luxSensor;
extern StateMachine stateMachine;

void SensorManager::begin() {
    float initialLux = luxSensor.lightStrengthLux();
    if (initialLux < 0) initialLux = 0;
    for (int i = 0; i < LUX_HISTORY_SIZE; i++) {
        luxHistory[i] = initialLux;
    }
    Serial.println("[SensorManager] Buffer de lux inicializado. Sensor PM omitido.");
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
    // ===== TEMPERATURA Y HUMEDAD =====
    float rawTemp = hdc1080.readTemperature();
    float rawHum = hdc1080.readHumidity();

    if (rawTemp > HDC_TEMP_MIN && rawTemp < HDC_TEMP_MAX) {
        stateMachine.sensors.temp = rawTemp + TEMP_OFFSET;
        hdc1080ErrorCount = 0;
    } else {
        hdc1080ErrorCount++;
    }

    if (rawHum >= HDC_HUM_MIN && rawHum <= HDC_HUM_MAX) {
        stateMachine.sensors.hum = rawHum - HUM_OFFSET;
    }

    // ===== LUX =====
    float rawLux = luxSensor.lightStrengthLux();
    if (rawLux < 0 || rawLux > EXTREME_LUX_THRESHOLD) {
        stateMachine.sensors.lux = luxHistory[(luxHistoryIndex - 1 + LUX_HISTORY_SIZE) % LUX_HISTORY_SIZE];
    } else {
        float calibratedLux = (rawLux - LUX_CALIBRATION_OFFSET) / LUX_CALIBRATION_SLOPE;
        if (calibratedLux < 0) calibratedLux = 0.0f;
        luxHistory[luxHistoryIndex] = calibratedLux;
        luxHistoryIndex = (luxHistoryIndex + 1) % LUX_HISTORY_SIZE;
        stateMachine.sensors.lux = medianOf(luxHistory, LUX_HISTORY_SIZE);
    }

    // ===== RUIDO =====
    int rawADC = analogRead(SOUND_SENSOR_PIN);
    float voltage = rawADC * (VREF / 4096.0f);
    stateMachine.sensors.voltage = voltage;
    stateMachine.sensors.dbValue = voltage * 50.0f;

    // ===== ACUMULACIÓN PARA PROMEDIO =====
    accTemp += stateMachine.sensors.temp;
    accHum += stateMachine.sensors.hum;
    accLux += stateMachine.sensors.lux;
    accNoise += stateMachine.sensors.dbValue;
    sampleCount++;

    Serial.printf("Temp: %.1f C | Hum: %.1f %% | Lux: %.1f | Ruido: %.1f dBA | Muestras: %d\n",
                  stateMachine.sensors.temp, stateMachine.sensors.hum, stateMachine.sensors.lux, stateMachine.sensors.dbValue, sampleCount);
}

SensorData SensorManager::getAverages() {
    if (sampleCount == 0) return stateMachine.sensors;

    SensorData avg;
    avg.temp = accTemp / sampleCount;
    avg.hum = accHum / sampleCount;
    avg.lux = accLux / sampleCount;
    avg.dbValue = accNoise / sampleCount;

    return avg;
}

void SensorManager::resetAccumulator() {
    accTemp = 0.0f;
    accHum = 0.0f;
    accLux = 0.0f;
    accNoise = 0.0f;
    sampleCount = 0;
}