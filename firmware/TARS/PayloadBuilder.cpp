#include "PayloadBuilder.h"

#include <ArduinoJson.h>

String construirPayload(float temperatura, float humedad, float luz, float ruido,
                         uint16_t pm1_0, uint16_t pm2_5, uint16_t pm10) {
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

  JsonObject pm1 = doc["pm1_0"].to<JsonObject>();
  pm1["type"] = "Number";
  pm1["value"] = pm1_0;

  JsonObject pm25 = doc["pm2_5"].to<JsonObject>();
  pm25["type"] = "Number";
  pm25["value"] = pm2_5;

  JsonObject pm10obj = doc["pm10"].to<JsonObject>();
  pm10obj["type"] = "Number";
  pm10obj["value"] = pm10;

  String payload;
  serializeJson(doc, payload);
  return payload;
}