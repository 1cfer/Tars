#include "Estados.h"

#include <WiFiClientSecure.h>

#include "AppConfig.h"
#include "DevWebOTA.h"
#include "TokenManager.h"
#include "WiFiManager.h"

DevWebOTA *devWeb = nullptr;

// ========================================
// IMPLEMENTACIÓN ESTADO INICIO
// ========================================

void EstadoINICIO::onEnter() {
  Serial.println("===Entrando en Estado INICIO===");
  statemachine->flags.inicio = true;
  firstRun = true;
}

void EstadoINICIO::execute() {
  if (!firstRun) return;

  // Fase 5: Si es primer boot, forzar setup desde modo desarrollador
  if (!appConfig.isConfigured) {
    Serial.println("[INICIO] Primer boot detectado — forzando modo DESARROLLADOR para setup");
    statemachine->flags.dev = true;
    statemachine->ChangeState(new EstadoDESARROLLADOR());
    return;
  }

  if (statemachine->flags.dev) {
    Serial.println("Cambiando a Estado DESARROLLADOR desde INICIO");
    statemachine->ChangeState(new EstadoDESARROLLADOR());
    return;
  }

  unsigned long now = millis();
  Serial.println("Estado: INICIO");

  // Si esta en la zona donde se puede conectar a WiFi, intenta conectarse
  if (wifiManager.connect(16)) {
    Serial.println("[INICIO] WiFi listo para envío de datos");
    MDNS.begin(appConfig.hostname.c_str());
    Serial.printf("[mDNS] Activo en http://%s.local\n", appConfig.hostname.c_str());
  } else {
    Serial.println("[INICIO] Sin WiFi, autoReconnect activo");
  }

  statemachine->flags.inicio = false;
  statemachine->clocks.proximo_envio = now + appConfig.intervaloEnvio;
  statemachine->flags.envio_programado = true;

  statemachine->ChangeState(new EstadoLECTURA());
  firstRun = false;
}

void EstadoINICIO::onExit() {
  Serial.println("===Saliendo de Estado INICIO===");
  statemachine->flags.inicio = false;
  firstRun = false;
}

const char *EstadoINICIO::getName() { return "INICIO"; }

// ========================================
// IMPLEMENTACIÓN ESTADO LECTURA
// ========================================

void EstadoLECTURA::onEnter() {
  Serial.println("===Entrando en Estado LECTURA===");
  statemachine->flags.lectura = true;
  statemachine->flags.inicio = false;
  statemachine->flags.envio = false;
}

void EstadoLECTURA::execute() {
  unsigned long now = millis();

  if (statemachine->flags.dev) {
    Serial.println("Cambiando a Estado DESARROLLADOR desde LECTURA");
    statemachine->ChangeState(new EstadoDESARROLLADOR());
    return;
  }

  if (statemachine->isDisplayOn && (now - statemachine->clocks.ultima_interaccion > appConfig.tiempoInactividad)) {
    display.clearDisplay();
    display.display();
    statemachine->isDisplayOn = false;
    Serial.println("Pantalla apagada por inactividad");
  }

  if (statemachine->needsUpdate && statemachine->isDisplayOn) {
    updateDisplay();
    statemachine->needsUpdate = false;
  }

  if (now - statemachine->clocks.tiempo_lectura >= appConfig.intervaloLectura) {
    Serial.println("Estado: LECTURA");
    sensorManager.read();
    if (statemachine->isDisplayOn) {
      updateDisplay();
    }
    statemachine->clocks.tiempo_lectura = now;
  }

  if (statemachine->flags.envio_programado && (long)(now - statemachine->clocks.proximo_envio) >= 0) {
    statemachine->ChangeState(new EstadoENVIO());
    return;
  }

  // Mantener conexión WiFi activa (reconexión no-bloqueante cada 30s)
  wifiManager.maintainConnection();
}

void EstadoLECTURA::onExit() {
  Serial.println("===Saliendo de Estado LECTURA===");
  statemachine->flags.lectura = false;
}

const char *EstadoLECTURA::getName() { return "LECTURA"; }

// ========================================
// IMPLEMENTACIÓN ESTADO ENVIO
// ========================================

void EstadoENVIO::onEnter() {
  Serial.println("=== ENTRANDO A ESTADO: ENVIO ===");
  statemachine->flags.envio = true;
  statemachine->flags.inicio = false;
  statemachine->flags.lectura = false;

  if (statemachine->isDisplayOn) {
    display.clearDisplay();
    displayStateInfo("ENVIO");
    display.display();
  }
}

void EstadoENVIO::execute() {
  unsigned long now = statemachine->clocks.tiempo_actual;

  if (statemachine->flags.dev) {
    Serial.println("Cambiando a Estado DESARROLLADOR desde ENVIO");
    statemachine->ChangeState(new EstadoDESARROLLADOR());
    return;
  }

  if (!wifiManager.isConnected()) {
    Serial.println("Sin conexión WiFi, posponiendo envío");
    statemachine->clocks.proximo_envio = now + appConfig.intervaloReintento;
    statemachine->flags.envio_programado = true;
    statemachine->ChangeState(new EstadoLECTURA());
    return;
  }

  if (appConfig.useAgent) {
    Serial.println("[ENVIO] Modo agente activo, enviando sin token...");

    SensorData avg = sensorManager.getAverages();
    sensorManager.resetAccumulator();

    HTTPClient httpAgent;
    httpAgent.begin(appConfig.agentUrl);
    httpAgent.addHeader("Content-Type", "application/json");

    String agentPayload = construirPayload(avg.temp, avg.hum, avg.lux, avg.dbValue,
                                        avg.pm1_0, avg.pm2_5, avg.pm10);

    Serial.println("[AGENTE] Payload:");
    Serial.println(agentPayload);

    int agentCode = httpAgent.POST(agentPayload);

    if (agentCode >= 200 && agentCode < 300) {
      Serial.printf("✓ Envío al agente exitoso, código: %d\n", agentCode);
    } else {
      Serial.printf("✗ Error al enviar al agente, código: %d\n", agentCode);
    }

    httpAgent.end();

    statemachine->clocks.proximo_envio = now + appConfig.intervaloEnvio;
    statemachine->flags.envio_programado = true;
    statemachine->ChangeState(new EstadoLECTURA());
    return;
  }

  // Preguntamos por el token antes de intentar enviar, si no es valido o no se puede renovar, se pospone envio
  if (!appConfig.skipToken && !tokenManager.ensureValidToken()) {
    Serial.println("[ENVIO] No se pudo obtener token, posponiendo envío");
    statemachine->clocks.proximo_envio = now + appConfig.intervaloReintento;
    statemachine->flags.envio_programado = true;
    statemachine->ChangeState(new EstadoLECTURA());
    return;
  }

  Serial.println("Estado: ENVIO");
  Serial.printf("[ENVIO] Free heap: %u bytes\n", ESP.getFreeHeap());

  // Construir payload ANTES de abrir conexión HTTP (JsonDocument se libera al retornar)
  SensorData avg = sensorManager.getAverages();
  sensorManager.resetAccumulator();

  String payload = construirPayload(avg.temp, avg.hum, avg.lux, avg.dbValue,
                                   avg.pm1_0, avg.pm2_5, avg.pm10);
  Serial.println("[ENVIO] Payload JSON:");
  Serial.println(payload);

  // WiFiClientSecure en heap para no desbordar el stack (~16KB de buffers TLS)
  WiFiClientSecure *client = new WiFiClientSecure();
  if (!client) {
    Serial.println("[ENVIO] Error: sin memoria para WiFiClientSecure");
    statemachine->clocks.proximo_envio = now + appConfig.intervaloReintento;
    statemachine->flags.envio_programado = true;
    statemachine->ChangeState(new EstadoLECTURA());
    return;
  }
  client->setInsecure();

  HTTPClient http;
  http.begin(*client, appConfig.serverUrl);
  http.addHeader("Content-Type", "application/json");
  if (!appConfig.skipToken) {
    http.addHeader("Authorization", "Bearer " + tokenManager.getToken());
  }

  int httpResponseCode = http.PATCH(payload);

  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    Serial.printf("✓ Envío exitoso, código: %d\n", httpResponseCode);
  } else if (httpResponseCode == 401) {
    Serial.println("✗ Token rechazado (401), forzando renovación");
    tokenManager.clear();
    http.end();
    delete client;

    if (tokenManager.ensureValidToken()) {
      WiFiClientSecure *retryClient = new WiFiClientSecure();
      if (retryClient) {
        retryClient->setInsecure();
        HTTPClient retryhttp;
        retryhttp.begin(*retryClient, appConfig.serverUrl);
        retryhttp.addHeader("Content-Type", "application/json");
        retryhttp.addHeader("Authorization", "Bearer " + tokenManager.getToken());

        int retrycode = retryhttp.PATCH(payload);
        if (retrycode >= 200 && retrycode < 300) {
          Serial.printf("✓ Reintento exitoso, código: %d\n", retrycode);
        } else {
          Serial.printf("✗ Reintento fallido, código: %d\n", retrycode);
        }
        retryhttp.end();
        delete retryClient;
      }
    }
    statemachine->clocks.proximo_envio = now + appConfig.intervaloEnvio;
    statemachine->flags.envio_programado = true;
    statemachine->ChangeState(new EstadoLECTURA());
    return; // Evitar doble ChangeState
  } else {
    Serial.printf("✗ Error en envío: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  delete client;
  statemachine->clocks.proximo_envio = now + appConfig.intervaloEnvio;
  statemachine->flags.envio_programado = true;
  statemachine->ChangeState(new EstadoLECTURA());
}

void EstadoENVIO::onExit() {
  Serial.println("=== SALIENDO DE ESTADO: ENVIO ===");
  statemachine->flags.envio = false;
}

const char *EstadoENVIO::getName() { return "ENVIO"; }

// ========================================
// IMPLEMENTACIÓN ESTADO DESARROLLADOR
// ========================================

void EstadoDESARROLLADOR::onEnter() {
  Serial.println("=== ENTRANDO A ESTADO: DESARROLLADOR ===");
  statemachine->flags.dev = true;
  statemachine->flags.inicio = false;
  statemachine->flags.lectura = false;
  primera_vez = true;

  displayDeveloperInfo();

  if (wifiManager.isConnected()) MDNS.begin(appConfig.hostname.c_str());
}

void EstadoDESARROLLADOR::execute() {
  if (primera_vez) {
    if (!devWeb) {
      devWeb = new DevWebOTA(&server);
    }
    devWeb->begin();
    primera_vez = false;
  }

  devWeb->handle();

  // Refrescar pantalla cada 2s
  unsigned long now = millis();
  if (now - lastDisplayRefresh >= 2000) {
    displayDeveloperInfo();
    lastDisplayRefresh = now;
  }

  // Verificar si el botón pidió salir
  if (!statemachine->flags.dev) {
    Serial.println("Saliendo del modo desarrollador por botón...");
    statemachine->ChangeState(new EstadoINICIO());
    return;
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "exit") {
      Serial.println("Saliendo del modo desarrollador...");
      statemachine->flags.dev = false;
      statemachine->ChangeState(new EstadoINICIO());
      return;
    }
  }
}

void EstadoDESARROLLADOR::onExit() {
  Serial.println("=== SALIENDO DE ESTADO: DESARROLLADOR ===");
  statemachine->flags.dev = false;
  primera_vez = true;
  if (devWeb) {
    devWeb->end();
  }
}

const char *EstadoDESARROLLADOR::getName() { return "DESARROLLADOR"; }