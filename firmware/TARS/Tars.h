#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "AmeriProfile.h"
namespace Tars {
constexpr unsigned DESTINATIONS=4, QUEUE_LIMIT=32;
constexpr int SDA_PIN=21,SCL_PIN=22,LUX_ENABLE_PIN=5,SOUND_PIN=35,BUTTON_PIN=27;
struct Destination {
 bool enabled=false;String url,method="POST",auth="none",bearer,tokenUrl,user,password,clientId,clientSecret,service,path="/",ca;
};
struct Config {
 uint32_t version=1,readMs=5000,sendMs=60000,retryMs=20000,displayMs=60000,pageMs=0,controlPollMs=30000,cpuMHz=80,brightness=32;
 bool wifiSleep=true,otaEnabled=true;
 String id="new",ssid,password,controlUrl,controlKey,apPassword,ca,ntpServer="pool.ntp.org",ntpFallback="time.cloudflare.com";
 double tempOffset=-3,humOffset=-7,luxOffset=6.1551,luxSlope=1.3788,noiseSlope=50,noiseOffset=0;
 bool noiseVerified=false;
 Destination destinations[DESTINATIONS];
};
struct Reading {
 float temp=NAN,hum=NAN,lux=NAN,voltage=NAN,noise=NAN;
 uint64_t uptimeUs=0;int64_t utc=0;uint32_t failures=0;
};
struct Delivery { uint32_t pending=0,dropped=0,failures=0;int code=0;int64_t lastOk=0;uint64_t nextUs=0; };
extern Config config;extern Reading reading;extern Delivery delivery[DESTINATIONS];
extern SemaphoreHandle_t mutex;extern bool storageReady;
extern volatile bool sending,localUpload;extern volatile uint32_t requestedMaintenance;
struct Lock {Lock(){xSemaphoreTakeRecursive(mutex,portMAX_DELAY);}~Lock(){xSemaphoreGiveRecursive(mutex);} };
uint64_t nowUs();String hex(const uint8_t*,size_t);String sha(const String&);String randomHex(size_t);bool equal(const String&,const String&);
bool clockReady();bool validUrl(const String&,bool base=false);bool validHeader(const String&);
Config snapshot();uint32_t configVersion();bool loadConfig();bool applyConfig(const String&,String&);String configJson(const Config&,bool secrets);
String diagnostics();String manifest();void beginSensors();void sensorTask(void*);
void beginNetwork();void networkTask(void*);bool enqueue(const String&);uint32_t pendingCount();bool persistBoundary();
void beginMaintenance();void maintenanceLoop();bool maintenanceActive();uint32_t maintenanceRemaining();
void controlTick(const Config&);void networkSuccess(bool control,unsigned slot,uint32_t version);void configTick();bool configProbation();
void requestMaintenance(uint32_t);void requestRecovery(uint32_t);void oledPage();String buildPayload(const Config&,const Reading&,uint32_t,const String&,uint64_t,uint32_t,const uint32_t*);
}
