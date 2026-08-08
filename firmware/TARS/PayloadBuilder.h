#ifndef PAYLOADBUILDER_H
#define PAYLOADBUILDER_H

#include <Arduino.h>

// Payload para Orion Context Broker y Agente Flask
String construirPayload(float temperatura, float humedad, float luz, float ruido,
                         uint16_t pm1_0, uint16_t pm2_5, uint16_t pm10);
#endif
