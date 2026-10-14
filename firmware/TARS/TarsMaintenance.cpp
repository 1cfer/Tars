#include "Tars.h"
#include "AmeriOta.h"
#include "TarsPage.h"
#include "Sha256Page.h"
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <mbedtls/sha256.h>
namespace Tars {
namespace {
WebServer web(80);uint64_t until=0;portMUX_TYPE deadlineMux=portMUX_INITIALIZER_UNLOCKED;bool maintenanceAp=false,forcedAp=false;String csrf,error,expectedHash;uint64_t rebootAt=0;
uint64_t deadline(){portENTER_CRITICAL(&deadlineMux);uint64_t t=until;portEXIT_CRITICAL(&deadlineMux);return t;}
void deadline(uint64_t t){portENTER_CRITICAL(&deadlineMux);until=t;portEXIT_CRITICAL(&deadlineMux);}
bool authorized=false,uploadOK=false,hashOpen=false;uint32_t size=0,received=0;uint8_t first[288];size_t firstCount=0;mbedtls_sha256_context hash;
bool auth(bool mutate=false){
 Config c=snapshot();IPAddress local=web.client().localIP();
 bool viaSta=WiFi.status()==WL_CONNECTED&&local==WiFi.localIP();
 bool viaAp=maintenanceAp&&local==WiFi.softAPIP();
 if(!maintenanceActive()||(!viaSta&&!viaAp)){web.send(403,"text/plain","Mantenimiento cerrado o interfaz no autorizada.");return false;}
 if(!web.authenticate("admin",c.apPassword.c_str())){web.requestAuthentication(DIGEST_AUTH,"TARS");return false;}
 if(mutate&&!equal(web.header("X-CSRF-Token"),csrf)){web.send(403,"text/plain","Sesion invalida.");return false;}web.sendHeader("Cache-Control","no-store");return true;
}
void abortUpload(const String&s){error=s;Update.abort();localUpload=false;if(hashOpen){mbedtls_sha256_free(&hash);hashOpen=false;}}
void stopAp(){if(maintenanceAp){WiFi.softAPdisconnect(true);WiFi.mode(WIFI_STA);maintenanceAp=false;Serial.println("[MANTENIMIENTO] AP apagado.");}}
bool startAp(){if(maintenanceAp)return true;Config c=snapshot();String id=c.id=="new"?"NEW-"+WiFi.macAddress().substring(9):c.id;id.replace(":","");
 WiFi.mode(WIFI_AP_STA);if(!WiFi.softAP(("TARS-"+id).c_str(),c.apPassword.c_str())){WiFi.mode(WIFI_STA);Serial.println("[MANTENIMIENTO] No se pudo crear el AP.");return false;}
 maintenanceAp=true;Serial.printf("[MANTENIMIENTO] AP TARS-%s; http://192.168.4.1\n",id.c_str());return true;
}
void close(){if(localUpload)abortUpload("Mantenimiento expirado");web.stop();stopAp();deadline(0);forcedAp=false;csrf="";Serial.println("[MANTENIMIENTO] Panel cerrado.");}
void open(uint32_t seconds,bool force=false){if(AmeriOta::active()||localUpload)return;
 if(!force&&WiFi.status()==WL_CONNECTED){stopAp();Serial.printf("[MANTENIMIENTO] Panel http://%s; sin AP.\n",WiFi.localIP().toString().c_str());}
 else if(!startAp())return;
 forcedAp=force;csrf=randomHex(24);deadline(nowUs()+uint64_t(seconds)*1000000);web.begin();
}
void upload(){HTTPUpload&u=web.upload();
 if(u.status==UPLOAD_FILE_START){authorized=auth(true);uploadOK=false;error="";received=0;firstCount=0;if(!authorized)return;
  if(localUpload||AmeriOta::active()){error="Otra OTA esta activa";return;}
  expectedHash=web.header("X-Firmware-SHA256");String length=web.header("X-Firmware-Size");size=0;bool valid=length.length()>0&&length.length()<9;for(char ch:length){if(ch<'0'||ch>'9')valid=false;else size=size*10+ch-'0';}
  bool hexOK=expectedHash.length()==64;for(char ch:expectedHash)if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))hexOK=false;
  auto slot=esp_ota_get_next_update_partition(nullptr);if(!valid||!hexOK||!slot||size<288||size>slot->size){error="Tamano, SHA256 o particion invalidos";return;}
  if(!Update.begin(size)){error=Update.errorString();return;}mbedtls_sha256_init(&hash);mbedtls_sha256_starts(&hash,0);hashOpen=true;localUpload=true;
 }else if(u.status==UPLOAD_FILE_WRITE&&localUpload){
  if(!maintenanceActive()||u.currentSize>size-received){abortUpload("Tamano excedido o acceso expirado");return;}
  if(firstCount<sizeof first){size_t n=min(u.currentSize,sizeof first-firstCount);memcpy(first+firstCount,u.buf,n);firstCount+=n;if(firstCount==sizeof first){auto*h=reinterpret_cast<esp_image_header_t*>(first);auto*d=reinterpret_cast<esp_app_desc_t*>(first+32);if(h->magic!=ESP_IMAGE_HEADER_MAGIC||h->chip_id!=AMERI_CHIP_ID||d->magic_word!=ESP_APP_DESC_MAGIC_WORD||strnlen(d->project_name,32)>=32||String(d->project_name)!=AMERI_PROJECT){abortUpload("Imagen ajena: requiere TARS para ESP32 clasico");return;}}}
  mbedtls_sha256_update(&hash,u.buf,u.currentSize);if(Update.write(u.buf,u.currentSize)!=u.currentSize){abortUpload(Update.errorString());return;}received+=u.currentSize;
 }else if(u.status==UPLOAD_FILE_END&&localUpload){
  uint8_t b[32];mbedtls_sha256_finish(&hash,b);mbedtls_sha256_free(&hash);hashOpen=false;
  if(received!=size||firstCount!=sizeof first||!equal(hex(b,32),expectedHash)||!maintenanceActive()){abortUpload("La imagen no coincide con tamano o SHA256");return;}
  if(!persistBoundary()){abortUpload("No se pudo cerrar y persistir la medicion");return;}
  if(!Update.end(false)){abortUpload(Update.errorString());return;}uploadOK=true;localUpload=false;rebootAt=nowUs()+1500000;
 }else if(u.status==UPLOAD_FILE_ABORTED&&localUpload)abortUpload("Carga interrumpida; firmware anterior conservado");
}
}
// Bit 31 reserva una peticion fisica de AP forzado; no altera el protocolo del servidor.
void requestMaintenance(uint32_t seconds){requestedMaintenance=seconds;}
void requestRecovery(uint32_t seconds){if(seconds>=60&&seconds<=900)requestedMaintenance=seconds|0x80000000U;}
bool maintenanceActive(){uint64_t t=deadline();return t&&nowUs()<t;}
uint32_t maintenanceRemaining(){uint64_t t=deadline(),n=nowUs();return t>n?uint32_t((t-n)/1000000):0;}
void beginMaintenance(){const char*headers[]={"X-CSRF-Token","X-Firmware-Size","X-Firmware-SHA256"};web.collectHeaders(headers,3);
 web.on("/",HTTP_GET,[](){if(!auth())return;String p=FPSTR(TARS_PAGE);p.replace("__CSRF__",csrf);p.replace("__SHA_JS__",String("<script>")+FPSTR(SHA256_PAGE_JS)+"</script>");web.send(200,"text/html; charset=utf-8",p);});
 web.on("/config",HTTP_GET,[](){if(auth())web.send(200,"application/json",configJson(snapshot(),false));});
 web.on("/config",HTTP_POST,[](){if(!auth(true))return;if(localUpload||AmeriOta::active()){web.send(409,"text/plain","OTA activa");return;}String e;bool ok=applyConfig(web.arg("plain"),e);web.send(ok?200:409,"text/plain",ok?"Configuracion completa guardada en el equipo.":e);});
 web.on("/status",HTTP_GET,[](){if(auth())web.send(200,"application/json",diagnostics());});
 web.on("/manifest",HTTP_GET,[](){if(auth())web.send(200,"application/json",manifest());});
 web.on("/scan",HTTP_GET,[](){if(!auth())return;int n=WiFi.scanComplete();if(n==-2){WiFi.scanNetworks(true);web.send(202,"text/plain","Buscando; pulse otra vez en unos segundos.");return;}if(n==-1){web.send(202,"text/plain","Busqueda en curso.");return;}JsonDocument d;auto a=d["networks"].to<JsonArray>();for(int i=0;i<n&&i<30;++i){auto o=a.add<JsonObject>();o["ssid"]=WiFi.SSID(i);o["rssi"]=WiFi.RSSI(i);o["open"]=WiFi.encryptionType(i)==WIFI_AUTH_OPEN;}String s;serializeJson(d,s);WiFi.scanDelete();web.send(200,"application/json",s);});
 web.on("/close",HTTP_POST,[](){if(!auth(true))return;web.send(200,"text/plain","Mantenimiento cerrado");close();});
 web.on("/update",HTTP_POST,[](){if(authorized)web.send(uploadOK?200:400,"text/plain",uploadOK?"Firmware verificado. Reiniciando; compruebe version y ultimo contacto.":error);},upload);
 web.onNotFound([](){web.send(404,"text/plain","Ruta inexistente");});
}
void maintenanceLoop(){
 if(requestedMaintenance){uint32_t seconds=requestedMaintenance;requestedMaintenance=0;if(seconds==UINT32_MAX)close();else {bool force=(seconds&0x80000000U)!=0;seconds&=0x7fffffffU;if(seconds>=60&&seconds<=900)open(seconds,force);}}
 uint64_t end=deadline();if(end&&nowUs()>=end)close();
 if(maintenanceActive()){
  // Cambiar de interfaz no renueva la sesion ni interrumpe una carga OTA en curso.
  if(!localUpload){if(!forcedAp&&WiFi.status()==WL_CONNECTED)stopAp();else if(!maintenanceAp){static uint64_t nextAp=0;if(nowUs()>=nextAp){nextAp=nowUs()+10000000;startAp();}}}
  web.handleClient();
 }
 if(rebootAt&&nowUs()>=rebootAt)ESP.restart();
}
}
