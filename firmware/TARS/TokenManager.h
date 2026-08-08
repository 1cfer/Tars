#ifndef TOKENMANAGER_H
#define TOKENMANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "AppConfig.h"

class TokenManager {
 private:
  String token;
  unsigned long tokenExpiry = 0;

 public:
  // Metodo para solicitar el token
  bool requestToken() {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[TokenManager] Sin WiFi, no se puede pedir token");
      return false;
    }

    // Iniciamos el cliente HTTP para solicitar el token por medio de la url configurada desde la web
    HTTPClient http;
    http.begin(appConfig.tokenUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Construimos el cuerpo de la solicitud del token, segun la documentacion de fiware este es la forma 2 de solicitar el token y es con las credenciales en el body
    // Si ya el servidor quedaria alojado en internet hay que cambiar a la forma 1 por seguridad, que es con las credenciales del client codificadas en base64 en el
    // header
    String body = "grant_type=password";
    body += "&username=" + appConfig.keyrockUser;
    body += "&password=" + appConfig.keyrockPass;
    body += "&client_id=" + appConfig.clientId;
    body += "&client_secret=" + appConfig.clientSecret;

    Serial.println("[TokenManager] Solicitando token...");
    Serial.printf("[TokenManager] URL: %s\n", appConfig.tokenUrl.c_str());
    Serial.printf("[TokenManager] Usuario: %s\n", appConfig.keyrockUser.c_str());

    // Enviamos la solicitud POST para obtener el token
    int httpCode = http.POST(body);

    if (httpCode >= 200 && httpCode < 300) {
      String response = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, response);

      if (!error && doc.containsKey("access_token")) {
        token = doc["access_token"].as<String>();
        unsigned long expiresIn = doc["expires_in"] | 3600;

        // Renovamos el token 60 segundos antes para no enviar con un token muerto
        tokenExpiry = millis() + (expiresIn - 60) * 1000;

        Serial.printf("[TokenManager] Token obtenido! Expira en %lu segundos", expiresIn);
        Serial.printf("[TokenManager] Token: %s...\n", token.substring(0, 20).c_str());
        http.end();
        return true;
      } else {
        Serial.println("[TokenManager] Error al parsear respuesta del token");
        Serial.println(response);
      }
    } else {
      Serial.printf("[TokenManager] Error HTTP: %d\n", httpCode);
      Serial.println(http.getString());
    }

    http.end();
    return false;
  }

  // Metodo para verificar si el token es valido y renovarlo si no lo es
  bool ensureValidToken() {
    if (token.length() > 0 && millis() < tokenExpiry) {
      return true;
    }
    Serial.println("[TokenManager] Token expirado o no existe, renovando...");
    return requestToken();
  }

  // Metodo para obtener el token
  String getToken() { return token; }

  // Metodo para verificar si el token es valido sin pedir nuevo
  bool hasToken() { return token.length() > 0 && millis() < tokenExpiry; }

  // Metodo para limpiar el token manualmente, se creo para la web de desarrollador para forzar la renovación del token
  void clear() {
    token = "";
    tokenExpiry = 0;
    Serial.println("[TokenManager] Token limpiado");
  }
};

extern TokenManager tokenManager;

#endif
