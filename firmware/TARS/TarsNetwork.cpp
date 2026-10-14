#include "Tars.h"
#include "AmeriJson.h"
#include "AmeriOta.h"
#include <SPIFFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_partition.h>
#include <time.h>
namespace Tars {
bool storageReady=false;volatile bool sending=false,localUpload=false;volatile uint32_t requestedMaintenance=0;
namespace {
Preferences store;uint64_t ticket=0,reserved=0;String cachedToken[DESTINATIONS];uint64_t expires[DESTINATIONS]={};uint32_t authVersion=0;
String prefix(unsigned slot){return "/q"+String(slot)+"-";}
String oldest(unsigned slot){File dir=SPIFFS.open("/");String best;for(File f=dir.openNextFile();f;f=dir.openNextFile()){String n=f.name();if(!n.startsWith("/"))n="/"+n;if(n.startsWith(prefix(slot))&&n.endsWith(".json")&&(!best.length()||n<best))best=n;}return best;}
String firstPayload(unsigned slot,String&path){Lock lock;path=oldest(slot);if(!path.length())return "";File f=SPIFFS.open(path,"r");size_t left=f.size();if(!left||left>8192)return "";String s;if(!s.reserve(left))return "";uint8_t bytes[512];while(left){size_t n=f.read(bytes,min(left,sizeof bytes));if(!n||!s.concat(reinterpret_cast<const char*>(bytes),n))return "";left-=n;}f.close();JsonDocument d,p;if(!ameriParseObject(d,s)||!d["payload_json"].is<String>())return "";String payload=d["payload_json"].as<String>();if(!equal(sha(payload),d["sha256"].as<String>())||!ameriParseObject(p,payload))return "";return payload;}
String encoded(const String&s){String r;const char*h="0123456789ABCDEF";for(uint8_t c:s){if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')r+=char(c);else {r+='%';r+=h[c>>4];r+=h[c&15];}}return r;}
class Sink:public Stream{public:String body;bool overflow=false;size_t write(uint8_t b)override{return write(&b,1);}size_t write(const uint8_t*b,size_t n)override{size_t k=min(n,size_t(24576-body.length()));if(k&&!body.concat(reinterpret_cast<const char*>(b),k))overflow=true;if(k<n)overflow=true;return n;}int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}};
}
// Todos los canales usan TLS validado, reloj UTC y redirecciones desactivadas.
int request(const Config&c,const String&url,const String&method,const String&body,const Destination*x,String&response,const String&type="application/json",const String&token=""){
 if(!clockReady())return -1001;if(!validUrl(url)||WiFi.status()!=WL_CONNECTED)return -1002;
 String ca=(x&&x->ca.length())?x->ca:c.ca;WiFiClientSecure tls;tls.setCACert(ca.c_str());tls.setHandshakeTimeout(8);HTTPClient http;
 if(!http.begin(tls,url))return -1003;http.setConnectTimeout(5000);http.setTimeout(5000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);http.addHeader("Content-Type",type);
 if(token.length())http.addHeader("Authorization","Bearer "+token);
 if(x){if(x->service.length())http.addHeader("Fiware-Service",x->service);if(x->path.length())http.addHeader("Fiware-ServicePath",x->path);}
 int code=http.sendRequest(method.c_str(),body);if(code>0){Sink sink;http.writeToStream(&sink);if(sink.overflow)code=-1004;else response=sink.body;}http.end();return code;
}
bool tokenFor(const Config&c,unsigned i,String&token){const auto&x=c.destinations[i];if(x.auth=="none")return true;if(x.auth=="bearer"){token=x.bearer;return token.length();}
 if(nowUs()<expires[i]&&cachedToken[i].length()){token=cachedToken[i];return true;}
 String body="grant_type=password&username="+encoded(x.user)+"&password="+encoded(x.password)+"&client_id="+encoded(x.clientId)+"&client_secret="+encoded(x.clientSecret),response;
 int code=request(c,x.tokenUrl,"POST",body,&x,response,"application/x-www-form-urlencoded");JsonDocument d;
 if(code!=200||!ameriParseObject(d,response)||!d["access_token"].is<String>()||!d["expires_in"].is<uint32_t>())return false;
 uint32_t ttl=d["expires_in"].as<uint32_t>();token=d["access_token"].as<String>();if(!ttl||ttl>86400||!token.length()||!validHeader(token))return false;
 cachedToken[i]=token;expires[i]=nowUs()+uint64_t(ttl-min(uint32_t(60),ttl/10))*1000000;return true;
}
bool enqueue(const String&payload){Lock lock;if(!storageReady)return false;bool all=true;JsonDocument record;record["payload_json"]=payload;record["sha256"]=sha(payload);String stored;serializeJson(record,stored);
 for(unsigned i=0;i<DESTINATIONS;++i){if(!config.destinations[i].enabled)continue;auto&d=delivery[i];if(d.pending>=QUEUE_LIMIT||SPIFFS.totalBytes()-SPIFFS.usedBytes()<payload.length()+8192){++d.dropped;store.putUInt(("drop"+String(i)).c_str(),d.dropped);all=false;continue;}
  if(ticket>=reserved){if(reserved>UINT64_MAX-1024){all=false;continue;}uint64_t next=reserved+1024;if(store.putULong64("reserve",next)!=8){all=false;continue;}reserved=next;}++ticket;
  char n[48];snprintf(n,sizeof n,"/q%u-%020llu.json",i,(unsigned long long)ticket);String tmp=String(n)+".tmp";File f=SPIFFS.open(tmp,"w");bool ok=f&&f.print(stored)==stored.length();f.flush();f.close();
  if(ok&&SPIFFS.rename(tmp,n))++d.pending;else {SPIFFS.remove(tmp);++d.dropped;store.putUInt(("drop"+String(i)).c_str(),d.dropped);all=false;}
 }return all;
}
uint32_t pendingCount(){Lock lock;uint32_t n=0;for(auto&d:delivery)n+=d.pending;return n;}
void beginNetwork(){
 if(!store.begin("tars4-outbox",false))return;ticket=reserved=store.getULong64("reserve",0);
 storageReady=SPIFFS.begin(false);
 if(!storageReady){const auto*p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);bool blank=p!=nullptr;uint8_t b[1024];
  // No formatea un sistema existente: comprueba TODA la particion antes de crear.
  if(p)for(uint32_t off=0;off<p->size&&blank;off+=sizeof b){if(esp_partition_read(p,off,b,min(sizeof b,size_t(p->size-off)))!=ESP_OK){blank=false;break;}for(size_t j=0;j<min(sizeof b,size_t(p->size-off));++j)if(b[j]!=0xff){blank=false;break;}}
  if(blank)storageReady=SPIFFS.begin(true);
 }
 if(storageReady){File dir=SPIFFS.open("/");for(File f=dir.openNextFile();f;f=dir.openNextFile()){String n=f.name();if(!n.startsWith("/"))n="/"+n;for(unsigned i=0;i<DESTINATIONS;++i)if(n.startsWith(prefix(i))&&n.endsWith(".json"))++delivery[i].pending;}}
 for(unsigned i=0;i<DESTINATIONS;++i)delivery[i].dropped=store.getUInt(("drop"+String(i)).c_str(),0);
}
void networkTask(void*){String ssid,password;bool init=true,sleep=false;uint64_t nextWifi=0,nextControl=0;unsigned slot=0;Config c=snapshot();
 for(;;){if(c.version!=configVersion()){c=snapshot();nextControl=0;}uint64_t now=nowUs();
  if(init||c.ssid!=ssid||c.password!=password){ssid=c.ssid;password=c.password;WiFi.disconnect(false,false);WiFi.setAutoReconnect(true);WiFi.persistent(false);if(ssid.length())WiFi.begin(ssid.c_str(),password.c_str());nextWifi=now+30000000;init=false;}
  if(sleep!=c.wifiSleep){WiFi.setSleep(c.wifiSleep);sleep=c.wifiSleep;}
  if(WiFi.status()!=WL_CONNECTED&&ssid.length()&&now>=nextWifi){WiFi.reconnect();nextWifi=now+30000000;}
  if(c.version!=authVersion){for(unsigned i=0;i<DESTINATIONS;++i){cachedToken[i]="";expires[i]=0;}authVersion=c.version;}
  if(!localUpload&&!AmeriOta::active())pairingTick(c);if(c.version!=configVersion())continue;
  if(WiFi.status()==WL_CONNECTED&&clockReady()&&!localUpload){
   if(now>=nextControl&&!AmeriOta::active()){controlTick(c);nextControl=nowUs()+uint64_t(c.controlPollMs)*1000;if(c.version!=configVersion())continue;}
   unsigned i=slot++%DESTINATIONS;String path,payload;bool due;
   {Lock lock;due=c.destinations[i].enabled&&delivery[i].pending&&nowUs()>=delivery[i].nextUs;}
   if(due&&(payload=firstPayload(i,path)).length()){
    sending=true;String token,response;int code=tokenFor(c,i,token)?request(c,c.destinations[i].url,c.destinations[i].method,payload,&c.destinations[i],response,"application/json",token):-1005;sending=false;
    bool success=false;
    {Lock lock;auto&d=delivery[i];d.code=code;
     // Una respuesta a la version anterior nunca modifica una cola con otra ruta.
     if(code>=200&&code<300&&config.destinations[i].url==c.destinations[i].url){if(SPIFFS.remove(path)){if(d.pending)--d.pending;d.failures=0;d.lastOk=time(nullptr);d.nextUs=nowUs()+100000;success=true;}else d.code=-1006;}
     if(!success){++d.failures;uint64_t delay=min(max(uint64_t(300000),uint64_t(c.retryMs)),uint64_t(c.retryMs)*(1ULL<<min(d.failures-1,uint32_t(4))));d.nextUs=nowUs()+(delay+esp_random()%1000)*1000;}
    }
    if(code==401||code==403){cachedToken[i]="";expires[i]=0;}
    if(success)networkSuccess(false,i,c.version);
   }else if(due){Lock lock;delivery[i].code=-1007;delivery[i].nextUs=nowUs()+300000000;}
  }
  vTaskDelay(pdMS_TO_TICKS(50));
 }
}
String diagnostics(){Config c=snapshot();JsonDocument d;{Lock lock;d["device_id"]=c.id;d["physical_id"]=WiFi.macAddress();d["config_version"]=c.version;d["storage_ready"]=storageReady;d["clock_ready"]=clockReady();d["wifi_connected"]=WiFi.status()==WL_CONNECTED;d["sta_ip"]=WiFi.localIP().toString();d["wifi_rssi"]=WiFi.status()==WL_CONNECTED?WiFi.RSSI():0;d["uptime_seconds"]=nowUs()/1000000;d["free_heap"]=ESP.getFreeHeap();d["minimum_heap"]=ESP.getMinFreeHeap();d["maintenance_active"]=maintenanceActive();d["remaining_seconds"]=maintenanceRemaining();d["network_probation"]=configProbation();d["firmware_version"]=AMERI_VERSION;d["sensor_invalid_mask"]=reading.failures;d["last_read_uptime_us"]=reading.uptimeUs;
 auto a=d["destinations"].to<JsonArray>();for(unsigned i=0;i<DESTINATIONS;++i){auto o=a.add<JsonObject>();auto&x=delivery[i];o["slot"]=i;o["enabled"]=c.destinations[i].enabled;o["pending_records"]=x.pending;o["dropped_records"]=x.dropped;o["http_status"]=x.code;o["last_success_at"]=x.lastOk;o["consecutive_failures"]=x.failures;}}
 {Lock lock;auto v=d["latest_reading"].to<JsonObject>();v["temperature"]=reading.temp;v["humidity"]=reading.hum;v["illuminance"]=reading.lux;v["noise_voltage"]=reading.voltage;v["noise_estimate_db"]=reading.noise;v["uptime_us"]=reading.uptimeUs;}
 d["time_utc"]=clockReady()?int64_t(time(nullptr)):0;d["config"]=serialized(configJson(c,false));d["manifest"]=serialized(manifest());d["ota"]=serialized(AmeriOta::diagnostics());d["pairing"]=serialized(pairingStatus());String out;serializeJson(d,out);return out;
}
}
