#ifndef PAYLOADBUILDER_H
#define PAYLOADBUILDER_H
#include <Arduino.h>

String construirPayload(float temperatura, float humedad, float luz, float ruido);

#endif