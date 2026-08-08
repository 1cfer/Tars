#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <Arduino.h>
#include <Preferences.h>

class AppConfig {
 private:
  Preferences prefs;

 public:
  // ===== CONFIGURACION DE ENVIO DE DATOS (ORION) =====
  String serverUrl = "http://10.38.35.216:1026/v2/entities/tarsdev/attrs";
  String hostname = "tars-new";  // Default genérico, se configura en primer boot
  unsigned long intervaloEnvio = 15000;
  unsigned long intervaloLectura = 2000;
  unsigned long intervaloReintento = 20000;
  unsigned long tiempoInactividad = 180000;

  // ===== CREDENCIALES KEYROCK =====
  String tokenUrl = "http://10.38.35.216:3001/oauth2/token";
  String clientId = "d4eac061-b057-45ff-87b2-f317275c3f58";
  String clientSecret = "f8e1bbf2-cbe9-44fb-a500-5bc6d60d17c7";
  String keyrockUser = "Tarst_v1@gmail.com";
  String keyrockPass = "123";
  bool skipToken = false;

  // ===== CONFIGURACION AGENTE =====
  String agentUrl = "http://10.38.35.216:5000/v1/agent/tars1";
  bool useAgent = false;

  // ===== PRIMER BOOT =====
  bool isConfigured = false;  // false = primer boot, forzar setup desde OTA

  void begin() {
    prefs.begin("appconfig", false);
    isConfigured = prefs.getBool("isConfigured", isConfigured);
    serverUrl = prefs.getString("serverUrl", serverUrl.c_str());
    hostname = prefs.getString("hostname", hostname.c_str());
    intervaloEnvio = prefs.getULong("intervaloEnvio", intervaloEnvio);
    intervaloLectura = prefs.getULong("intervaloLectura", intervaloLectura);
    intervaloReintento = prefs.getULong("intervaloReintento", intervaloReintento);
    tiempoInactividad = prefs.getULong("tiempoInactividad", tiempoInactividad);

    tokenUrl = prefs.getString("tokenUrl", tokenUrl.c_str());
    clientId = prefs.getString("clientId", clientId.c_str());
    clientSecret = prefs.getString("clientSecret", clientSecret.c_str());
    keyrockUser = prefs.getString("keyrockUser", keyrockUser.c_str());
    keyrockPass = prefs.getString("keyrockPass", keyrockPass.c_str());
    skipToken = prefs.getBool("skipToken", skipToken);

    agentUrl = prefs.getString("agentUrl", agentUrl.c_str());
    useAgent = prefs.getBool("useAgent", useAgent);
    prefs.end();

    Serial.println("[AppConfig] Configuracion cargada:");
    Serial.printf("  isConfigured: %s\n", isConfigured ? "true" : "false");
    Serial.printf("  hostname: %s\n", hostname.c_str());
    Serial.printf("  serverUrl: %s\n", serverUrl.c_str());
    Serial.printf("  intervaloEnvio: %lu ms\n", intervaloEnvio);
    Serial.printf("  intervaloLectura: %lu ms\n", intervaloLectura);
    Serial.printf("  agentUrl: %s\n", agentUrl.c_str());
    Serial.printf("  useAgent: %s\n", useAgent ? "true" : "false");
    Serial.printf("  skipToken: %s\n", skipToken ? "true" : "false");
  }

  void save() {
    prefs.begin("appconfig", false);
    prefs.putBool("isConfigured", true);  // Si se guarda config, ya está configurado
    prefs.putString("serverUrl", serverUrl.c_str());
    prefs.putString("hostname", hostname.c_str());
    prefs.putULong("intervaloEnvio", intervaloEnvio);
    prefs.putULong("intervaloLectura", intervaloLectura);
    prefs.putULong("intervaloReintento", intervaloReintento);
    prefs.putULong("tiempoInactividad", tiempoInactividad);

    prefs.putString("tokenUrl", tokenUrl.c_str());
    prefs.putString("clientId", clientId.c_str());
    prefs.putString("clientSecret", clientSecret.c_str());
    prefs.putString("keyrockUser", keyrockUser.c_str());
    prefs.putString("keyrockPass", keyrockPass.c_str());
    prefs.putBool("skipToken", skipToken);

    prefs.putString("agentUrl", agentUrl.c_str());
    prefs.putBool("useAgent", useAgent);
    prefs.end();
    isConfigured = true;
    Serial.println("[AppConfig] Configuracion guardada en NVS");
  }

  void reset() {
    prefs.begin("appconfig", false);
    prefs.clear();
    prefs.end();
    serverUrl = "http://10.38.35.216:1026/v2/entities/tarsdev/attrs";
    hostname = "tars-new";
    intervaloEnvio = 15000;
    intervaloLectura = 2000;
    intervaloReintento = 20000;
    tiempoInactividad = 180000;

    tokenUrl = "http://10.38.35.216:3001/oauth2/token";
    clientId = "d4eac061-b057-45ff-87b2-f317275c3f58";
    clientSecret = "f8e1bbf2-cbe9-44fb-a500-5bc6d60d17c7";
    keyrockUser = "Tarst_v1@gmail.com";
    keyrockPass = "123";
    skipToken = false;

    agentUrl = "http://10.38.35.216:5000/v1/agent/tars1";
    useAgent = false;
    isConfigured = false;
    Serial.println("[AppConfig] Configuracion restaurada a defaults");
  }
};

extern AppConfig appConfig;

#endif