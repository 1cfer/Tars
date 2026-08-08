#ifndef ESTADOS_H
#define ESTADOS_H

#include <Adafruit_SSD1306.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>

#include "AppConfig.h"
#include "DisplayManager.h"
#include "PayloadBuilder.h"
#include "SensorManager.h"
#include "State.h"
#include "StateMachine.h"
#include "TokenManager.h"
#include "WiFiManager.h"

// Objetos definidos en TARS.ino
extern Adafruit_SSD1306 display;
extern WebServer server;

// ========================================
// ESTADO INICIO
// ========================================
class EstadoINICIO : public State {
 private:
  bool firstRun = true;

 public:
  void onEnter() override;
  void execute() override;
  void onExit() override;
  const char* getName() override;
};

// ========================================
// ESTADO LECTURA
// ========================================
class EstadoLECTURA : public State {
 public:
  void onEnter() override;
  void execute() override;
  void onExit() override;
  const char* getName() override;
};

// ========================================
// ESTADO ENVIO
// ========================================
class EstadoENVIO : public State {
 public:
  void onEnter() override;
  void execute() override;
  void onExit() override;
  const char* getName() override;
};

// ========================================
// ESTADO DESARROLLADOR
// ========================================
class EstadoDESARROLLADOR : public State {
 private:
  bool primera_vez = true;
  unsigned long lastDisplayRefresh = 0;

 public:
  void onEnter() override;
  void execute() override;
  void onExit() override;
  const char* getName() override;
};

#endif
