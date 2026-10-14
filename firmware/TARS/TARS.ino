#include "Tars.h"
#include "AmeriIdentity.h"
#include "AmeriOta.h"
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <time.h>
SET_LOOP_TASK_STACK_SIZE(16384);
String serialCommand;
void serialDiagnostics(){
 while(Serial.available()){char ch=Serial.read();if(ch=='\r'||ch=='\n'){
  serialCommand.trim();if(serialCommand.equalsIgnoreCase("WIFI")){
   auto c=Tars::snapshot();String id=c.id=="new"?"NEW-"+WiFi.macAddress().substring(9):c.id;id.replace(":","");
   Serial.printf("[MANTENIMIENTO] Usuario admin; clave: %s\n",c.apPassword.c_str());
   if(WiFi.status()==WL_CONNECTED)Serial.printf("Pagina en la red actual: http://%s\n",WiFi.localIP().toString().c_str());
   else Serial.printf("AP: TARS-%s; pagina: http://192.168.4.1\n",id.c_str());
   Tars::requestMaintenance(600);
  }serialCommand="";
 }else if(serialCommand.length()<32)serialCommand+=ch;}
}
bool started=false,otaStarted=false;uint64_t bootAt=0,nextOtaAttempt=0;
void setup(){
 Serial.begin(115200);Tars::mutex=xSemaphoreCreateRecursiveMutex();
 if(!Tars::mutex){AmeriOta::rollbackIfPending();return;}
 WiFi.mode(WIFI_STA);WiFi.persistent(false);WiFi.setSleep(true);
 if(!Tars::loadConfig()){Serial.println("[TARS] Configuracion invalida; no se borra ni sustituye.");AmeriOta::rollbackIfPending();return;}
 Tars::beginPairing();Tars::beginNetwork();Tars::beginSensors();Tars::beginMaintenance();
 configTime(0,0,Tars::config.ntpServer.c_str(),Tars::config.ntpFallback.c_str());
 TaskHandle_t sensors=nullptr,network=nullptr;
 if(xTaskCreatePinnedToCore(Tars::sensorTask,"tars-sensors",8192,nullptr,2,&sensors,1)!=pdPASS||xTaskCreatePinnedToCore(Tars::networkTask,"tars-network",16384,nullptr,1,&network,0)!=pdPASS){Serial.println("[TARS] No se pudieron crear tareas.");AmeriOta::rollbackIfPending();return;}
 bootAt=Tars::nowUs();started=true;
 // Primera configuracion vacia: AP inicial; equipos configurados conservan mantenimiento temporal.
 if(Tars::snapshot().id=="new"&&!Tars::snapshot().ssid.length())Tars::requestRecovery(600);
 Serial.println("[TARS] 4.1.1: Escriba WIFI + Enter en serie para abrir mantenimiento y consultar su clave local.");
 Serial.println("[TARS] Arranque; diagnostico sin credenciales. Boton 4s: mantenimiento. Boton 10s: AP de recuperacion 10min.");
}
void loop(){if(!started){delay(100);return;}
 serialDiagnostics();Tars::maintenanceLoop();Tars::configTick();static Tars::Config c=Tars::snapshot();if(c.version!=Tars::configVersion()){c=Tars::snapshot();configTime(0,0,c.ntpServer.c_str(),c.ntpFallback.c_str());}
 if(Ameri::identity.bound&&!otaStarted&&Tars::nowUs()>=nextOtaAttempt){nextOtaAttempt=Tars::nowUs()+60000000;otaStarted=AmeriOta::begin();}
 Tars::Reading r;{Tars::Lock lock;r=Tars::reading;}
 bool sensorOK=r.uptimeUs&&Tars::nowUs()-r.uptimeUs<uint64_t(c.readMs)*2000&&(r.failures&7)!=7;
 if(otaStarted){AmeriOta::Safety s(Tars::maintenanceActive()||Tars::localUpload||Tars::configProbation(),true,Tars::storageReady&&sensorOK,Tars::pendingCount());s.otaEnabled=c.otaEnabled;AmeriOta::Network n;n.base=Tars::otaBase(c);n.ca=c.ca;AmeriOta::tick(s,n,Tars::persistBoundary);}
 else { // Primera migracion por OTA clasica, aun sin clave de control provisionada.
  esp_ota_img_states_t state;if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY){
   if(Tars::storageReady&&sensorOK&&Tars::nowUs()-bootAt>10000000)esp_ota_mark_app_valid_cancel_rollback();
   else if(Tars::nowUs()-bootAt>90000000)AmeriOta::rollbackIfPending();
  }
 }
 static uint64_t lastLog=0;if(Tars::nowUs()-lastLog>=10000000){lastLog=Tars::nowUs();Serial.printf("[ESTADO] uptime=%lus muestra=%lluus fallos=0x%X wifi=%s AP=%s RAM=%u\n",millis()/1000,(unsigned long long)r.uptimeUs,r.failures,WiFi.status()==WL_CONNECTED?"OK":"DESCONECTADO",((WiFi.getMode() & WIFI_AP)!=0)?"ON":"OFF",ESP.getFreeHeap());}
 delay(50);
}
