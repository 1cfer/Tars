#include "DisplayManager.h"

#include <Adafruit_SSD1306.h>
#include <WiFi.h>

#include "AppConfig.h"
#include "StateMachine.h"
#include "WiFiManager.h"

extern Adafruit_SSD1306 display;
extern StateMachine stateMachine;
extern WiFiManager wifiManager;

void drawAllSensors() {
  display.setTextSize(0);
  display.setCursor(15, 17);
  display.println("Sensores:");

  display.setCursor(0, 23);
  display.print("Temp:      ");
  display.print(stateMachine.sensors.temp, 1);
  display.println(" C");

  display.setCursor(0, 32);
  display.print("Hum:       ");
  display.print(stateMachine.sensors.hum, 1);
  display.println(" %");

  display.setCursor(0, 41);
  display.print("Lux:       ");
  display.print(stateMachine.sensors.lux, 1);
  display.println(" lux");

  display.setCursor(0, 51);
  display.print("Ruido:     ");
  display.print(stateMachine.sensors.dbValue, 1);
  display.println(" dBA");
}

void updateDisplay() {
  if (!stateMachine.isDisplayOn) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  displayStateInfo("LECTURA");

  switch (stateMachine.screenMode) {
    case 0:
      drawAllSensors();
      break;

    case 1:
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("TEMP/HUM:");

      display.setTextSize(1);
      display.setCursor(0, 35);
      display.print(stateMachine.sensors.temp, 1);
      display.println(" C TEMP");

      display.setCursor(0, 45);
      display.print(stateMachine.sensors.hum, 1);
      display.println(" % HUM");
      break;

    case 2:
      display.setTextSize(1);
      display.setCursor(0, 17);
      display.println("LUZ:");
      display.setTextSize(2);
      display.print(stateMachine.sensors.lux, 1);
      display.println(" lux");
      break;

    case 3:
      display.setTextSize(1);
      display.setCursor(0, 17);
      display.println("RUIDO:");

      display.setTextSize(2);
      display.setCursor(0, 27);
      display.print(stateMachine.sensors.dbValue, 1);
      display.println(" dBA");

      display.setTextSize(1);
      display.setCursor(0, 50);
      display.print("V: ");
      display.print(stateMachine.sensors.voltage, 3);
      display.println(" V");
      break;
    case 4:  // NUEVO: calidad de aire
      display.setTextSize(1);
      display.setCursor(0, 17);
      display.println("CALIDAD AIRE:");

      display.setCursor(0, 30);
      display.print("PM1.0:  ");
      display.print(stateMachine.sensors.pm1_0);
      display.println(" ug/m3");

      display.setCursor(0, 40);
      display.print("PM2.5:  ");
      display.print(stateMachine.sensors.pm2_5);
      display.println(" ug/m3");

      display.setCursor(0, 50);
      display.print("PM10:   ");
      display.print(stateMachine.sensors.pm10);
      display.println(" ug/m3");
      break;
  }

  display.display();
}

void displayStateInfo(const char* estado) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Estado: ");
  display.println(estado);
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  display.setCursor(0, 9);
  if (WiFi.status() == WL_CONNECTED) {
    display.println(wifiManager.getIP());
  } else {
    display.println("Sin conexion");
  }
}

void displayDeveloperInfo() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Línea 0: Título (zona amarilla del SSD1306)
  display.setCursor(0, 0);
  display.println("DESARROLLADOR");
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // Línea 1: IP
  display.setCursor(0, 12);
  display.print("IP:");
  display.println(wifiManager.getIP());

  // Línea 2: Red WiFi o modo AP
  display.setCursor(0, 22);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("Red:");
    String ssid = WiFi.SSID();
    if (ssid.length() > 17) ssid = ssid.substring(0, 17);
    display.println(ssid);
  } else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    display.print("AP:");
    String apName = "TARS-" + appConfig.hostname;
    if (apName.length() > 18) apName = apName.substring(0, 18);
    display.println(apName);
  } else {
    display.println("Sin conexion");
  }

  // Línea 3: Estado de agente y keyrock
  display.setCursor(0, 32);
  display.print("Ag:");
  display.print(appConfig.useAgent ? "ON " : "OFF");
  display.print(" Key:");
  display.print(appConfig.skipToken ? "OFF" : "ON ");
  display.print(" E:");
  if (appConfig.intervaloEnvio >= 60000) {
    display.print(appConfig.intervaloEnvio / 60000);
    display.println("m");
  } else {
    display.print(appConfig.intervaloEnvio / 1000);
    display.println("s");
  }

  display.drawLine(0, 41, 128, 41, SSD1306_WHITE);

  // Línea 4: Hostname (truncado si es largo)
  display.setCursor(0, 44);
  String hostLine = appConfig.hostname;
  if (hostLine.length() > 15) hostLine = hostLine.substring(0, 15);
  display.print(hostLine);
  display.println(".local");

  // Línea 5: Señal WiFi
  display.setCursor(0, 54);
  display.print("RSSI:");
  display.print(WiFi.RSSI());
  display.print("dBm Heap:");
  display.print(ESP.getFreeHeap() / 1024);
  display.println("K");

  display.display();
}
