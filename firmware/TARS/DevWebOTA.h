#ifndef DEVWEBOTA_H
#define DEVWEBOTA_H

#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

class DevWebOTA {
 private:
  WebServer *server;
  bool initialized;
  bool routesRegistered;

 public:
  DevWebOTA(WebServer *srv);

  void begin();
  void handle();
  void end();
};

#endif
