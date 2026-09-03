#ifndef TOKENMANAGER_H
#define TOKENMANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "AppConfig.h"

class TokenManager {
 private:
  String token;
  unsigned long tokenAcquiredMs = 0;
  unsigned long tokenTtlMs = 0;

 public:
  bool requestToken() {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[TokenManager] Sin WiFi, no se puede pedir token");
      return false;
    }

    // 1. Blindaje de RAM: Evitar colapso si la memoria libre es muy baja
    uint32_t freeMem = ESP.getFreeHeap();
    if (freeMem < 25000) {
      Serial.printf("[TokenManager] Heap critico (%u bytes). Postergando solicitud.\n", freeMem);
      return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Ahorra RAM al no validar la cadena de certificados

    HTTPClient http;
    http.setTimeout(10000); // Timeout estricto de 10s para evitar cuelgues

    Serial.println("[TokenManager] Solicitando nuevo token a Keyrock...");
    Serial.printf("[TokenManager] URL: %s\n", appConfig.tokenUrl.c_str());

    if (!http.begin(client, appConfig.tokenUrl)) {
      Serial.println("[TokenManager] Error al iniciar conexion con Keyrock");
      client.stop();
      return false;
    }

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "grant_type=password";
    body += "&username=" + appConfig.keyrockUser;
    body += "&password=" + appConfig.keyrockPass;
    body += "&client_id=" + appConfig.clientId;
    body += "&client_secret=" + appConfig.clientSecret;

    int httpCode = http.POST(body);

    if (httpCode >= 200 && httpCode < 300) {
      String response = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, response);

      if (!error && doc.containsKey("access_token")) {
        token = doc["access_token"].as<String>();
        unsigned long expiresIn = doc["expires_in"] | 3600;

        // Margen preventivo: si dura 1h (3600s), renueva a los 50 min (600s antes)
        unsigned long safetyMargin = (expiresIn > 600) ? 600 : 60;
        tokenTtlMs = (expiresIn - safetyMargin) * 1000UL;
        tokenAcquiredMs = millis();

        Serial.printf("[TokenManager] Token obtenido! TTL util: %lu s (Margen: %lu s)\n", 
                      tokenTtlMs / 1000, safetyMargin);
        http.end();
        client.stop();
        return true;
      } else {
        Serial.println("[TokenManager] Error al parsear respuesta JSON");
        Serial.println(response);
      }
    } else {
      Serial.printf("[TokenManager] Error HTTP al pedir token: %d\n", httpCode);
      Serial.println(http.getString());
    }

    http.end();
    client.stop();
    return false;
  }

  // Verifica validez evitando errores por desbordamiento de millis()
  bool hasToken() {
    if (token.length() == 0) return false;
    return (millis() - tokenAcquiredMs) < tokenTtlMs;
  }

  bool ensureValidToken() {
    if (hasToken()) {
      return true;
    }
    Serial.println("[TokenManager] Token expirado o ausente. Solicitando renovacion...");
    return requestToken();
  }

  String getToken() { return token; }

  // Invalida el token inmediatamente ante un HTTP 401 / 403 / 302
  void clear() {
    token = "";
    tokenAcquiredMs = 0;
    tokenTtlMs = 0;
    Serial.println("[TokenManager] Token invalidado manualmente.");
  }
};

extern TokenManager tokenManager;

#endif