#include "Estados.h"
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>

#include "AppConfig.h" 
#include "DevWebOTA.h" 
#include "TokenManager.h" 
#include "WiFiManager.h"
#include "DisplayManager.h"

DevWebOTA *devWeb = nullptr;
extern Adafruit_SSD1306 display;
extern WebServer server;
extern SensorManager sensorManager;

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
    ButtonHandler::Event btn = statemachine->currentEvent;

    // CORRECCIÓN: % 4 PARA CICLAR LAS 4 PANTALLAS
    if (btn == ButtonHandler::SHORT_PRESS) {
        statemachine->screenMode = (statemachine->screenMode + 1) % 4; 
        statemachine->needsUpdate = true;
    } else if (btn == ButtonHandler::VERY_LONG_PRESS_4S) {
        statemachine->ChangeState(new EstadoDESARROLLADOR());
        return;
    }

    if (statemachine->isDisplayOn && (now - statemachine->clocks.ultima_interaccion > appConfig.tiempoInactividad)) { 
        display.clearDisplay(); 
        display.display(); 
        statemachine->isDisplayOn = false; 
    }

    if (statemachine->needsUpdate && statemachine->isDisplayOn) { 
        updateDisplay(); 
        statemachine->needsUpdate = false; 
    }

    if (now - statemachine->clocks.tiempo_lectura >= appConfig.intervaloLectura) { 
        sensorManager.read(); 
        if (statemachine->isDisplayOn) { updateDisplay(); } 
        statemachine->clocks.tiempo_lectura = now; 
    }

    if (statemachine->flags.envio_programado && (long)(now - statemachine->clocks.proximo_envio) >= 0) { 
        statemachine->ChangeState(new EstadoENVIO()); 
        return; 
    }

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
    ButtonHandler::Event btn = statemachine->currentEvent;

    // Permitir ir al modo desarrollador si se traba el envío
    if (btn == ButtonHandler::VERY_LONG_PRESS_4S) {
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

    if (!appConfig.skipToken && !tokenManager.ensureValidToken()) { 
        Serial.println("[ENVIO] No se pudo obtener token, posponiendo envío"); 
        statemachine->clocks.proximo_envio = now + appConfig.intervaloReintento; 
        statemachine->flags.envio_programado = true; 
        statemachine->ChangeState(new EstadoLECTURA()); 
        return; 
    }

    SensorData avg = sensorManager.getAverages(); 
    sensorManager.resetAccumulator();

    // EXCLUIDO EL SENSOR PM: Construimos el payload solo con Temperatura, Humedad, Luz y Ruido
    String payload = construirPayload(avg.temp, avg.hum, avg.lux, avg.dbValue); 
    Serial.println("[ENVIO] Payload JSON:"); 
    Serial.println(payload);

    WiFiClientSecure *client = new WiFiClientSecure(); 
    if (!client) { 
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
        tokenManager.clear(); 
    }

    http.end(); 
    delete client; 
    statemachine->clocks.proximo_envio = now + appConfig.intervaloEnvio; 
    statemachine->flags.envio_programado = true; 
    statemachine->ChangeState(new EstadoLECTURA()); 
}

void EstadoENVIO::onExit() { 
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
    
    // Reiniciamos las variables de navegación al entrar
    subMode = 0;
    selectedNetwork = 0;
    scrollOffset = 0;
    totalNetworks = 0;

    displayDeveloperInfo();
    if (wifiManager.isConnected()) MDNS.begin(appConfig.hostname.c_str()); 
}

void EstadoDESARROLLADOR::execute() { 
    ButtonHandler::Event btn = statemachine->currentEvent;
    unsigned long now = millis();

    // ===== SUB-MODO 0: MENÚ PRINCIPAL =====
    if (subMode == 0) {
        if (primera_vez) {
            if (!devWeb) { devWeb = new DevWebOTA(&server); }
            devWeb->begin();
            primera_vez = false;
        }
        devWeb->handle();

        if (now - lastDisplayRefresh >= 2000) { 
            displayDeveloperInfo(); 
            lastDisplayRefresh = now; 
        }

        if (btn == ButtonHandler::VERY_LONG_PRESS_4S) {
            statemachine->ChangeState(new EstadoINICIO());
            return;
        } 
        else if (btn == ButtonHandler::SHORT_PRESS) {
            subMode = 1;
            WiFi.scanNetworks(true); 
            display.clearDisplay(); 
            display.setCursor(10,30); 
            display.print("Buscando WiFi..."); 
            display.display();
        }
    } 
    // ===== SUB-MODO 1: ESCANEANDO =====
    else if (subMode == 1) { 
        int n = WiFi.scanComplete();
        if (n >= 0) { 
            totalNetworks = n; 
            selectedNetwork = 0; 
            scrollOffset = 0; 
            subMode = 2; 
            displayWiFiList(selectedNetwork, scrollOffset, totalNetworks);
        }
    }
    // ===== SUB-MODO 2: LISTA PAGINADA =====
    else if (subMode == 2) { 
        // 4 Segundos ignorados a propósito
        if (btn == ButtonHandler::DOUBLE_CLICK) { 
            WiFi.scanDelete(); 
            subMode = 0; 
            displayDeveloperInfo();
        } 
        else if (btn == ButtonHandler::SHORT_PRESS) { 
            selectedNetwork++;
            if (selectedNetwork >= totalNetworks) selectedNetwork = 0;
            if (selectedNetwork >= scrollOffset + 5) scrollOffset = selectedNetwork - 4;
            if (selectedNetwork < scrollOffset) scrollOffset = selectedNetwork;
            displayWiFiList(selectedNetwork, scrollOffset, totalNetworks);
        } 
        else if (btn == ButtonHandler::LONG_PRESS_2S) { 
            String ssid = WiFi.SSID(selectedNetwork);
            // REVISAR EL LLAVERO CON EL NUEVO METODO
            if (wifiManager.hasCredentialsFor(ssid)) { 
                display.clearDisplay(); 
                display.setCursor(10,30); 
                display.print("Conectando..."); 
                display.display();
                wifiManager.connectTo(ssid); // Conectar a esa red específica
                subMode = 0; 
                displayDeveloperInfo();
            } else {
                subMode = 3;
                wifiManager.createAP(appConfig.hostname);
                MDNS.begin(appConfig.hostname.c_str());
                if (!devWeb) { devWeb = new DevWebOTA(&server); }
                server.begin(); 
                displayAPAlert();
            }
        }
    }
    // ===== SUB-MODO 3: ALERTA AP Y SERVIDOR LOCAL =====
    else if (subMode == 3) { 
        devWeb->handle();
        // 4 Segundos ignorados a propósito
        if (btn == ButtonHandler::DOUBLE_CLICK) { 
            subMode = 0; 
            displayDeveloperInfo(); 
            
            // Si el usuario configuró desde la web local, intentar conexión inmediatamente al salir
            if(WiFi.status() != WL_CONNECTED) {
                display.clearDisplay(); display.setCursor(10,30); 
                display.print("Aplicando Red..."); display.display();
                wifiManager.connect(5);
                displayDeveloperInfo(); 
            }
        }
    }
}

void EstadoDESARROLLADOR::onExit() { 
    statemachine->flags.dev = false; 
    primera_vez = true; 
    if (devWeb) { devWeb->end(); } 
}

const char *EstadoDESARROLLADOR::getName() { return "DESARROLLADOR"; }