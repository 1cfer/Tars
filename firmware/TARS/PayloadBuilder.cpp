#include "PayloadBuilder.h"
#include <ArduinoJson.h>

String construirPayload(float temperatura, float humedad, float luz, float ruido) {
    JsonDocument doc;

    JsonObject temperature = doc["temperature"].to<JsonObject>();
    temperature["type"] = "Number";
    temperature["value"] = serialized(String(temperatura, 1));

    JsonObject humidity = doc["humidity"].to<JsonObject>();
    humidity["type"] = "Number";
    humidity["value"] = serialized(String(humedad, 1));

    JsonObject illuminance = doc["illuminance"].to<JsonObject>();
    illuminance["type"] = "Number";
    illuminance["value"] = serialized(String(luz, 1));

    JsonObject noise = doc["noise"].to<JsonObject>();
    noise["type"] = "Number";
    noise["value"] = serialized(String(ruido, 1));

    String payload;
    serializeJson(doc, payload);
    return payload;
}