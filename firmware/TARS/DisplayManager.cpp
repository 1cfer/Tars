#include "DisplayManager.h"
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "AppConfig.h"
#include "StateMachine.h"
#include "WiFiManager.h"

extern Adafruit_SSD1306 display;
extern StateMachine stateMachine;
extern WiFiManager wifiManager;

// =========================================================
// HEADER CON PAGINACION INTEGRADA
// =========================================================
void drawScreenHeader(const char* title, int page = -1, int total = -1) {
    display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.print(title);
    
    if (page >= 0 && total >= 0) {
        display.setCursor(95, 2);
        display.print("["); display.print(page + 1); display.print("/"); display.print(total); display.print("]");
    } else {
        display.setCursor(105, 2);
        if (WiFi.status() == WL_CONNECTED) display.print("WiFi");
        else display.print("--");
    }
    display.setTextColor(SSD1306_WHITE);
}

// =========================================================
// PANTALLAS DE MODO LECTURA
// =========================================================
void drawAllSensors() {
    drawScreenHeader("TARS - Resumen", 0, 4);
    
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.print("Temp:  "); display.print(stateMachine.sensors.temp, 1); display.println(" C");

    display.setCursor(0, 28);
    display.print("Hum:   "); display.print(stateMachine.sensors.hum, 1); display.println(" %");

    display.setCursor(0, 40);
    display.print("Lux:   "); display.print(stateMachine.sensors.lux, 1); display.println(" lx");

    display.setCursor(0, 52);
    display.print("Ruido: "); display.print(stateMachine.sensors.dbValue, 1); display.println(" dBA");
}

void drawTempHumScreen() {
    drawScreenHeader("Temp / Humedad", 1, 4);
    
    display.setTextSize(2);
    display.setCursor(5, 18);
    display.print(stateMachine.sensors.temp, 1); display.setTextSize(1); display.print(" C");
    
    display.setTextSize(2);
    display.setCursor(5, 42);
    display.print(stateMachine.sensors.hum, 1); display.setTextSize(1); display.print(" %");
    
    display.drawRoundRect(75, 15, 50, 43, 3, SSD1306_WHITE);
    display.setCursor(82, 22); display.print("AMB.");
    display.setCursor(82, 40); display.print("ACTUAL");
}

void drawLuxScreen() {
    drawScreenHeader("Iluminacion", 2, 4);
    
    display.setTextSize(1);
    display.setCursor(10, 20); display.println("Intensidad (B_LUX):");
    
    display.setTextSize(2);
    display.setCursor(10, 35); display.print(stateMachine.sensors.lux, 1); 
    
    display.setTextSize(1);
    display.setCursor(10, 54); display.println("lux");
    
    // Sol pixel-art
    display.drawCircle(105, 40, 6, SSD1306_WHITE);
    display.drawLine(105, 29, 105, 32, SSD1306_WHITE);
    display.drawLine(105, 48, 105, 51, SSD1306_WHITE);
    display.drawLine(94, 40, 97, 40, SSD1306_WHITE);
    display.drawLine(113, 40, 116, 40, SSD1306_WHITE);
}

void drawNoiseScreen() {
    drawScreenHeader("Cont. Acustica", 3, 4);
    
    display.setTextSize(2);
    display.setCursor(10, 22);
    display.print(stateMachine.sensors.dbValue, 1); display.setTextSize(1); display.print(" dBA");
    
    display.setCursor(10, 48);
    display.print("ADC Vol: "); display.print(stateMachine.sensors.voltage, 3); display.println(" V");
}

void updateDisplay() {
    if (!stateMachine.isDisplayOn) return;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    switch (stateMachine.screenMode) {
        case 0: drawAllSensors(); break;
        case 1: drawTempHumScreen(); break;
        case 2: drawLuxScreen(); break;
        case 3: drawNoiseScreen(); break;
    }
    display.display();
}

// =========================================================
// PANTALLAS DEL SISTEMA Y MENÚS
// =========================================================
void displayStateInfo(const char* estado) {
    display.clearDisplay();
    drawScreenHeader("ESTADO SISTEMA");
    
    display.setCursor(0, 25);
    display.print("MODO: "); display.println(estado);
    
    display.setCursor(0, 45);
    display.print("IP: ");
    if (WiFi.status() == WL_CONNECTED) display.println(wifiManager.getIP());
    else display.println("Desconectado");
    
    display.display();
}

void displayDeveloperInfo() {
    display.clearDisplay();
    drawScreenHeader("MODO DEVELOPER");

    display.setTextSize(1);
    display.setCursor(0, 14); display.print("IP: "); display.println(wifiManager.getIP());

    display.setCursor(0, 24);
    if (WiFi.status() == WL_CONNECTED) {
        display.print("WIFI: ");
        String ssid = WiFi.SSID();
        if (ssid.length() > 15) ssid = ssid.substring(0, 15);
        display.println(ssid);
    } else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        display.print("AP: TARS-");
        String h = appConfig.hostname;
        if (h.length() > 10) h = h.substring(0, 10);
        display.println(h);
    } else {
        display.println("Red: Desconectado");
    }

    display.setCursor(0, 34);
    display.print("Ag:"); display.print(appConfig.useAgent ? "ON " : "OFF");
    display.print(" Tk:"); display.print(appConfig.skipToken ? "OFF" : "ON ");
    display.print(" T:");
    if (appConfig.intervaloEnvio >= 60000) { display.print(appConfig.intervaloEnvio / 60000); display.println("m"); } 
    else { display.print(appConfig.intervaloEnvio / 1000); display.println("s"); }

    display.setCursor(0, 44);
    String hostLine = appConfig.hostname;
    if (hostLine.length() > 12) hostLine = hostLine.substring(0, 12);
    display.print("Host: "); display.print(hostLine); display.println(".local");

    display.setCursor(0, 54);
    display.print("S:"); display.print(WiFi.RSSI());
    display.print("dBm | Mem:"); display.print(ESP.getFreeHeap() / 1024); display.println("K");

    display.display();
}

void displayWiFiList(int selected, int offset, int total) {
    display.clearDisplay();
    display.setTextSize(1);
    
    display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(2, 2);
    display.print("REDES WIFI: "); display.println(total);
    display.setTextColor(SSD1306_WHITE);

    int maxLines = 5; 
    for (int i = 0; i < maxLines && (i + offset) < total; i++) {
        int netIdx = i + offset;
        int yPos = 14 + (i * 10);
        
        display.setCursor(0, yPos);
        if (netIdx == selected) display.print(">");
        else display.print(" ");
        
        String ssid = WiFi.SSID(netIdx);
        if (wifiManager.hasCredentialsFor(ssid)) {
            display.print("*"); 
        } else { 
            display.print(" "); 
        }
        
        if (ssid.length() > 17) ssid = ssid.substring(0, 17);
        display.print(ssid);
    }
    
    // Barra de scroll a la derecha
    if (total > maxLines) {
        int barHeight = (maxLines * 48) / total;
        if (barHeight < 5) barHeight = 5;
        int barY = 13 + (offset * (48 - barHeight)) / (total - maxLines);
        display.drawRect(125, 13, 3, 50, SSD1306_WHITE);
        display.fillRect(125, barY, 3, barHeight, SSD1306_WHITE);
    }
    display.display();
}

void displayAPAlert() {
    display.clearDisplay();
    display.setTextSize(1);
    
    display.fillRect(0, 0, 128, 13, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(12, 3); 
    display.println("! SIN CONTRASENA !"); 
    
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 17); display.println("Red no almacenada");
    display.setCursor(0, 30); display.print("Configurar desde:");
    
    display.setCursor(0, 42); 
    display.print("Wi-Fi: TARS-"); display.println(appConfig.hostname);
    
    display.setCursor(0, 53); 
    display.print("http://"); display.print(appConfig.hostname); display.println(".local");
    
    display.display();
}