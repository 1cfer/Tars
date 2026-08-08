#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

class WiFiManager {
 private:
  Preferences prefs;

  // Access Point — password fija, SSID se genera dinámicamente en createAP()
  const char *apPass = "12345678";

  // Control de reconexión
  unsigned long lastReconnectAttempt = 0;
  static const unsigned long RECONNECT_INTERVAL = 30000; // 30s entre intentos

 public:
  // Conecta a WiFi usando credenciales de NVS. Bloqueante solo para arranque.
  // Retorna false si no hay credenciales o si falla la conexión.
  bool connect(int maxAttempts = 16) {
    prefs.begin("agevital", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    prefs.end();

    if (ssid.length() == 0) {
      Serial.println("[WiFiManager] No hay SSID configurado en NVS");
      return false;
    }

    Serial.printf("[WiFiManager] Conectando a: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);      // No escribir credenciales en flash (usamos NVS)
    WiFi.setAutoReconnect(true); // Reconexión automática del stack WiFi
    WiFi.setSleep(false);        // Desactivar modem sleep para conexión estable
    if (pass.length() > 0) {
      WiFi.begin(ssid.c_str(), pass.c_str());
    } else {
      WiFi.begin(ssid.c_str());  // Red abierta: sin contraseña
    }

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n[WiFiManager] Conectado! IP: %s | RSSI: %d dBm\n", WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
      return true;
    } else {
      Serial.println("\n[WiFiManager] Conexión fallida");
      WiFi.disconnect();
      return false;
    }
  }

  // Crea Access Point con nombre dinámico basado en hostname
  void createAP(const String &hostname) {
    String apSSID = "TARS-" + hostname;
    Serial.println("[WiFiManager] Creando Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID.c_str(), apPass);
    Serial.printf("[WiFiManager] SSID: %s\n", apSSID.c_str());
    Serial.printf("[WiFiManager] Password: %s\n", apPass);
    Serial.printf("[WiFiManager] IP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  // Verificar y mantener conexión WiFi — llamar periódicamente desde EstadoLECTURA
  void maintainConnection() {
    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();
    if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
    lastReconnectAttempt = now;

    Serial.printf("[WiFiManager] WiFi desconectado, intentando reconexión... (RSSI anterior: %d)\n", WiFi.RSSI());
    WiFi.reconnect();
  }

  void saveCredentials(const String &newSSID, const String &newPass) {
    prefs.begin("agevital", false);
    prefs.putString("ssid", newSSID);
    prefs.putString("pass", newPass);
    prefs.end();
    Serial.printf("[WiFiManager] Credenciales guardadas: %s\n", newSSID.c_str());
  }

  void reset() {
    prefs.begin("agevital", false);
    prefs.clear();
    prefs.end();
    Serial.println("[WiFiManager] Credenciales WiFi borradas");
  }

  bool hasCredentials() {
    prefs.begin("agevital", true);
    String ssid = prefs.getString("ssid", "");
    prefs.end();
    return ssid.length() > 0;
  }

  bool isConnected() { return WiFi.status() == WL_CONNECTED; }

  String getIP() {
    if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
    if (WiFi.getMode() == WIFI_AP) return WiFi.softAPIP().toString();
    return "No conectado";
  }

  String getSSID() {
    if (WiFi.status() == WL_CONNECTED) return WiFi.SSID();
    return "";
  }
};

extern WiFiManager wifiManager;

#endif