#include "AmeriOta.h"
#include "Tars.h"
#include "AmeriIdentity.h"
#include "AmeriProfile.h"
#include "AmeriJson.h"
#include "AmeriBootTrust.h"
#include "AmeriOtaVectors.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_flash.h>
#include <esp_app_desc.h>
#include <esp_secure_boot.h>
#include <esp_timer.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <time.h>
namespace AmeriOta {
namespace {
SemaphoreHandle_t mux=nullptr;TaskHandle_t task=nullptr;
Safety safety;Network network;volatile bool working=false,boundaryWanted=false,boundaryDone=false,boundaryPrepared=false,bootTrusted=false,wroteSinceBoundary=false,currentValid=false;
Preferences prefs;uint64_t counter=0,reserved=0;
String jobText,phase,errorCode="none";uint32_t progress=0;bool terminalAcked=false,serverReached=false;
int64_t bootUs=0;bool pendingBoot=false;uint8_t otaKey[32];
String hex(const uint8_t* p,size_t n){String s;s.reserve(n*2);const char* h="0123456789abcdef";for(size_t i=0;i<n;++i){s+=h[p[i]>>4];s+=h[p[i]&15];}return s;}
String sha(const String& s){uint8_t h[32];mbedtls_sha256(reinterpret_cast<const uint8_t*>(s.c_str()),s.length(),h,0);return hex(h,32);}
bool eq(const String& a,const String& b){if(a.length()!=b.length())return false;uint8_t diff=0;for(size_t i=0;i<a.length();++i)diff|=a[i]^b[i];return diff==0;}
bool hmac(const uint8_t* key,size_t len,const String& msg,uint8_t* out){return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),key,len,reinterpret_cast<const uint8_t*>(msg.c_str()),msg.length(),out)==0;}
String sign(const String& msg){uint8_t h[32];if(!hmac(otaKey,32,msg,h))return "";return hex(h,32);}
bool hexN(const String& s,size_t n){if(s.length()!=n)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
String canonical(const char* kind,const String& id,uint64_t c,int64_t stamp,const String& nonce,const String& body){return String("ameri-ota-1-")+kind+"\n"+id+"\n"+String((unsigned long long)c)+"\n"+String((long long)stamp)+"\n"+nonce+"\n"+sha(body);}
bool vectorTest(){
 uint8_t derived[32];if(!hmac(reinterpret_cast<const uint8_t*>(AMERI_TEST_CONTROL),strlen(AMERI_TEST_CONTROL),"AMERI-OTA-1",derived)||hex(derived,32)!=AMERI_TEST_DERIVED)return false;
 return true;
}
Safety snapshot(Network* n=nullptr){xSemaphoreTake(mux,portMAX_DELAY);Safety s=safety;if(n)*n=network;xSemaphoreGive(mux);return s;}
bool safe(){if(Tars::persistenceFault())return false;auto s=snapshot();return s.otaEnabled&&!s.busy&&s.valveClosed&&s.pending==0;}
bool terminal(){return phase=="confirmed"||phase=="failed"||phase=="rolled_back";}
bool save(){if(Tars::persistenceFault())return false;JsonDocument d;d["job"]=jobText;d["state"]=phase;d["progress"]=progress;d["error"]=errorCode;d["acked"]=terminalAcked;String raw;serializeJson(d,raw);bool ok=prefs.putString("context",raw)==raw.length();if(!ok)Tars::pausePersistence("ota_context_failed");return ok;}
bool state(const char* s,uint32_t p,const char* err="none"){phase=s;progress=max(progress,p);errorCode=err;terminalAcked=false;return save();}
uint64_t nextCounter(){
 if(Tars::persistenceFault())return 0;
 constexpr uint64_t limit=9007199254740991ULL;
 if(counter>=reserved){if(reserved>limit-128)return 0;uint64_t next=reserved+128;if(prefs.putULong64("reserved",next)!=8){Tars::pausePersistence("ota_counter_failed");return 0;}reserved=next;}
 if(counter>=limit)return 0;return ++counter;
}
class Sink:public Stream{public:String body;bool overflow=false;size_t write(uint8_t b)override{return write(&b,1);}size_t write(const uint8_t* b,size_t n)override{size_t keep=min(n,size_t(24576-body.length()));if(keep&&!body.concat(reinterpret_cast<const char*>(b),keep))overflow=true;if(keep<n)overflow=true;return n;}int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}};
bool verified(const String& raw,const String& request,JsonDocument& payload){
 JsonDocument r,q;if(!ameriParseObject(r,raw)||!ameriParseObject(q,request))return false;
 if(!r["protocol"].is<int>()||r["protocol"].as<int>()!=1||!r["counter"].is<uint64_t>()||!r["timestamp"].is<int64_t>()||!r["payload_json"].is<String>())return false;
 String id=r["device_id"].as<String>(),nonce=r["nonce"].as<String>(),body=r["payload_json"].as<String>();
 uint64_t c=r["counter"].as<uint64_t>();int64_t t=r["timestamp"].as<int64_t>(),now=time(nullptr);
 if(id!=Ameri::identity.id||c!=q["counter"].as<uint64_t>()||nonce!=q["nonce"].as<String>()||!hexN(nonce,32)||body.length()>8192||t<now-120||t>now+120)return false;
 if(!eq(sign(canonical("response",id,c,t,nonce,body)),r["signature"].as<String>()))return false;
 return ameriParseObject(payload,body);
}
bool exchange(const char* kind,const String& payload,JsonDocument& response){
 if(Tars::persistenceFault())return false;
 static String outstanding,oldPayload,oldKind;static int64_t created=0;
 if(WiFi.status()!=WL_CONNECTED||time(nullptr)<1704067200)return false;
 if(outstanding.length()&&time(nullptr)-created>115){outstanding="";oldPayload="";oldKind="";}
 bool matching=!outstanding.length()||(oldKind==kind&&oldPayload==payload);
 if(!outstanding.length()||time(nullptr)-created>115){
  uint64_t c=nextCounter();if(!c)return false;uint8_t n[16];esp_fill_random(n,16);String nonce=hex(n,16);created=time(nullptr);
  JsonDocument d;d["protocol"]=1;d["device_id"]=Ameri::identity.id;d["counter"]=c;d["timestamp"]=created;d["nonce"]=nonce;d["payload_json"]=payload;
  d["signature"]=sign(canonical(kind,Ameri::identity.id,c,created,nonce,payload));outstanding="";serializeJson(d,outstanding);oldPayload=payload;oldKind=kind;
 }
 Network n;snapshot(&n);WiFiClientSecure tls;tls.setCACert(n.ca.c_str());tls.setHandshakeTimeout(8);HTTPClient http;
 String url=n.base+"/api/ota/devices/"+Ameri::identity.id+"/"+oldKind;
 if(!n.base.startsWith("https://")||!http.begin(tls,url))return false;
 http.setConnectTimeout(5000);http.setTimeout(5000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);http.addHeader("Content-Type","application/json");
 int code=http.POST(outstanding);Sink sink;if(code==200)http.writeToStream(&sink);http.end();
 if(code!=200||sink.overflow||!verified(sink.body,outstanding,response)){
  // Permanent rejections need a new counter; transport failures retry exact bytes.
  if(code==400||code==403||code==409){outstanding="";oldPayload="";oldKind="";}
  return false;
 }
 outstanding="";oldPayload="";oldKind="";serverReached=true;return matching;
}
String pollBody(){
 auto s=snapshot();const esp_partition_t* slot=esp_ota_get_next_update_partition(nullptr);const auto* desc=esp_app_get_description();
 JsonDocument d;d["ota_supported"]=s.otaEnabled&&bootTrusted&&currentValid;d["rollback_supported"]=bootTrusted;d["clock_ready"]=time(nullptr)>=1704067200;
 d["valve_closed"]=s.valveClosed;d["busy"]=s.busy||s.pending>0||working||pendingBoot;d["pending_records"]=s.pending;
 d["family"]=AMERI_FAMILY;d["board"]=AMERI_BOARD;d["chip_id"]=AMERI_CHIP_ID;d["slot_bytes"]=slot?slot->size:0;d["version"]=desc->version;d["project"]=desc->project_name;
 String out;serializeJson(d,out);return out;
}
bool ack(){
 if(!jobText.length())return true;JsonDocument j;deserializeJson(j,jobText);String id=j["job_id"].as<String>();JsonDocument a;a["job_id"]=id;a["state"]=phase;a["progress"]=progress;a["error_code"]=errorCode;
 if(phase=="confirmed"){a["version"]=esp_app_get_description()->version;a["sha256"]=j["sha256"];a["self_test_ok"]=true;a["boot_valid"]=true;}
 String body;serializeJson(a,body);JsonDocument response;if(!exchange("ack",body,response))return false;
 bool ok=response["acknowledged"].is<bool>()&&response["acknowledged"].as<bool>()&&response["job_id"].as<String>()==id&&response["state"].as<String>()==phase;
 if(ok&&terminal()){terminalAcked=true;save();}return ok;
}
bool inspectBoot(){
 #if !defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) || !defined(CONFIG_APP_ROLLBACK_ENABLE)
 return false;
 #endif
 if(esp_secure_boot_enabled())return false;
 auto* a=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,nullptr);
 auto* b=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,nullptr);
 auto* o=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,nullptr);
 if(!a||!b||!o||a->address!=0x10000||a->size!=0x140000||b->address!=0x150000||b->size!=0x140000||o->address!=0xe000||o->size!=0x2000)return false;
 uint8_t bytes[1024],digest[32];mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);mbedtls_sha256_starts(&hash,0);
 for(size_t off=0;off<AMERI_BOOT_BYTES;off+=sizeof(bytes)){size_t n=min(sizeof(bytes),size_t(AMERI_BOOT_BYTES-off));if(esp_flash_read(nullptr,bytes,AMERI_BOOT_OFFSET+off,n)!=ESP_OK){mbedtls_sha256_free(&hash);return false;}mbedtls_sha256_update(&hash,bytes,n);}
 mbedtls_sha256_finish(&hash,digest);mbedtls_sha256_free(&hash);return eq(hex(digest,32),AMERI_BOOT_SHA256);
}
bool command(const JsonDocument& d,const String& downloadToken){
 const auto* desc=esp_app_get_description();const auto* slot=esp_ota_get_next_update_partition(nullptr);Network n;snapshot(&n);
 String id=d["job_id"].as<String>(),release=d["release_id"].as<String>(),ver=d["version"].as<String>();
 if(!d["size"].is<uint32_t>()||!d["expires_at"].is<int64_t>()||!d["chip_id"].is<uint16_t>())return false;
 int64_t until=d["expires_at"].as<int64_t>();
 if(!bootTrusted||!currentValid||pendingBoot||!safe()||!slot||!hexN(id,32)||!hexN(release,32)||!hexN(d["sha256"].as<String>(),64)||!hexN(downloadToken,64)||d["family"].as<String>()!=AMERI_FAMILY||
 d["board"].as<String>()!=AMERI_BOARD||d["chip_id"].as<uint16_t>()!=AMERI_CHIP_ID||d["project"].as<String>()!=desc->project_name||ver==desc->version||!ver.length()||ver.length()>31||!Tars::acceptsConfigFirmware(ver.c_str())||
 d["size"].as<uint32_t>()<288||d["size"].as<uint32_t>()>slot->size||until<=time(nullptr)||until>time(nullptr)+900||
 d["url"].as<String>()!=n.base+"/api/ota/devices/"+Ameri::identity.id+"/firmware/"+id)return false;
 for(char c:ver)if(!isalnum(c)&&c!='_'&&c!='-'&&c!='.')return false;
 return true;
}
void install(JsonDocument& order){
 String token=order["download_token"].as<String>();if(!command(order,token))return;
 JsonDocument previous;if(jobText.length()&&!deserializeJson(previous,jobText)&&previous["job_id"].as<String>()==order["job_id"].as<String>())return;
 if(jobText.length()&&!terminalAcked)return;
 String consumed=prefs.getString("consumed","");String entry=order["job_id"].as<String>()+"\n";
 if(consumed.indexOf(entry)>=0||consumed.length()+entry.length()>4096)return;
 if(prefs.putString("consumed",consumed+entry)!=consumed.length()+entry.length()){Tars::pausePersistence("ota_replay_failed");return;}
 // Never store the bearer download permission. Keep only durable job metadata.
 order.remove("download_token");const auto* slot=esp_ota_get_next_update_partition(nullptr);order["target_address"]=slot->address;
 jobText="";serializeJson(order,jobText);progress=0;working=true;
 if(!state("accepted",0)){working=false;return;}
 if(!ack()){state("failed",0,"accept_ack_unavailable");working=false;return;}
 if(!state("downloading",0)){working=false;return;}
 ack();Network n;snapshot(&n);WiFiClientSecure tls;tls.setCACert(n.ca.c_str());tls.setHandshakeTimeout(8);HTTPClient http;
 bool started=false;esp_ota_handle_t handle=0;uint32_t size=order["size"].as<uint32_t>(),received=0;int64_t expires=order["expires_at"].as<int64_t>();
 const char* fail="download_failed";
 for(unsigned attempt=0;attempt<3&&time(nullptr)<expires&&safe();++attempt){
 received=0;started=false;fail="download_failed";
 if(http.begin(tls,order["url"].as<String>())){
  http.setConnectTimeout(5000);http.setTimeout(5000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);http.addHeader("Authorization","Bearer "+token);
  int code=http.GET();
  if(code==200&&http.getSize()==int(size)&&safe()&&esp_ota_begin(slot,size,&handle)==ESP_OK){
   started=true;mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);mbedtls_sha256_starts(&hash,0);
   uint8_t chunk[2048],first[288];size_t firstCount=0;bool headerOk=false;auto* stream=http.getStreamPtr();int64_t lastData=esp_timer_get_time(),lastProgress=lastData;
   while(received<size){
    if(!safe()||time(nullptr)>=expires){fail="activity_or_expired";break;}
    int available=stream->available();if(!available){if(!http.connected()||esp_timer_get_time()-lastData>5000000){fail="download_interrupted";break;}vTaskDelay(pdMS_TO_TICKS(5));continue;}
    int got=stream->readBytes(chunk,min(size_t(available),min(sizeof(chunk),size_t(size-received))));if(got<=0)continue;lastData=esp_timer_get_time();
    if(firstCount<sizeof(first)){size_t take=min(size_t(got),sizeof(first)-firstCount);memcpy(first+firstCount,chunk,take);firstCount+=take;
     if(firstCount==sizeof(first)){auto* h=reinterpret_cast<esp_image_header_t*>(first);auto* desc=reinterpret_cast<esp_app_desc_t*>(first+32);
      headerOk=h->magic==ESP_IMAGE_HEADER_MAGIC&&h->chip_id==AMERI_CHIP_ID&&desc->magic_word==ESP_APP_DESC_MAGIC_WORD&&
        strnlen(desc->version,32)<32&&strnlen(desc->project_name,32)<32&&String(desc->version)==order["version"].as<String>()&&String(desc->project_name)==AMERI_PROJECT;
      if(!headerOk){fail="image_descriptor_mismatch";break;}
     }
    }
    mbedtls_sha256_update(&hash,chunk,got);wroteSinceBoundary=true;if(esp_ota_write(handle,chunk,got)!=ESP_OK){fail="flash_write_failed";break;}received+=got;
    progress=max(progress,uint32_t(received*uint64_t(90)/size));
    if(esp_timer_get_time()-lastProgress>=5000000){save();ack();lastProgress=esp_timer_get_time();}
   }
   uint8_t digest[32];mbedtls_sha256_finish(&hash,digest);mbedtls_sha256_free(&hash);http.end();
   bool complete=received==size&&headerOk&&eq(hex(digest,32),order["sha256"].as<String>());
   if(!complete&&received==size)fail="sha256_mismatch";
   if(complete&&safe()&&state("verifying",95)){
    // esp_ota_end validates image checksum/chip/revision natively before boot switch.
    bool imageValid=esp_ota_end(handle)==ESP_OK;started=false;
    if(imageValid&&ack()){
     boundaryDone=false;boundaryPrepared=false;boundaryWanted=true;int64_t deadline=esp_timer_get_time()+45000000;
     while(!boundaryDone&&esp_timer_get_time()<deadline&&time(nullptr)<expires)vTaskDelay(pdMS_TO_TICKS(10));
     if(boundaryDone){while(!safe()&&esp_timer_get_time()<deadline&&time(nullptr)<expires)vTaskDelay(pdMS_TO_TICKS(10));}
     if(boundaryDone&&safe()&&state("rebooting",100)&&esp_ota_set_boot_partition(slot)==ESP_OK){ack();ESP.restart();}
     fail="boundary_or_boot_switch_failed";
    }else fail="native_image_or_ack_failed";
   }
  }
 }
 http.end();if(started){esp_ota_abort(handle);started=false;}
 if(String(fail)!="download_interrupted"&&String(fail)!="download_failed")break;
 if(attempt<2){int64_t retryAt=esp_timer_get_time()+2000000;while(esp_timer_get_time()<retryAt&&safe()&&time(nullptr)<expires)vTaskDelay(pdMS_TO_TICKS(20));}
 }
 token="";boundaryWanted=false;state("failed",progress,fail);working=false;
}
void run(void*){
 int64_t nextPoll=0,nextAck=0;String poll;
 for(;;){
  int64_t now=esp_timer_get_time();
  if(!currentValid){
   auto s=snapshot();bool good=s.selfTest&&(s.valveClosed||!Ameri::identity.actuatorVerified)&&WiFi.status()==WL_CONNECTED&&time(nullptr)>=1704067200&&serverReached&&now-bootUs>=10000000;
   if(good&&esp_ota_mark_app_valid_cancel_rollback()==ESP_OK){currentValid=true;pendingBoot=false;if(jobText.length()&&!terminal())state("confirmed",100);}
   else if(pendingBoot&&now-bootUs>90000000){esp_ota_mark_app_invalid_rollback_and_reboot();ESP.restart();}
  }
  if(jobText.length()&&!terminalAcked&&now>=nextAck){
   if(ack())nextAck=now+5000000;else nextAck=now+10000000;
  }
  if((snapshot().otaEnabled||pendingBoot)&&now>=nextPoll&&(!jobText.length()||terminalAcked||pendingBoot||!currentValid)){
   if(!poll.length())poll=pollBody();JsonDocument response;
   if(exchange("poll",poll,response)){poll="";nextPoll=now+60000000+esp_random()%10000000;
    if(!pendingBoot&&response["action"].as<String>()=="install")install(response);
   }else nextPoll=now+10000000+esp_random()%3000000;
  }
  vTaskDelay(pdMS_TO_TICKS(20));
 }
}
}
void rollbackIfPending(){
 esp_ota_img_states_t stateNow=ESP_OTA_IMG_UNDEFINED;
 if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&stateNow)==ESP_OK&&stateNow==ESP_OTA_IMG_PENDING_VERIFY){esp_ota_mark_app_invalid_rollback_and_reboot();ESP.restart();}
}
bool begin(){
 bootUs=esp_timer_get_time();if(!Ameri::identity.bound||!prefs.begin("ameri-ota",false)){rollbackIfPending();return false;}
 if(!vectorTest()){Serial.println("[OTA] HMAC fallo; canal bloqueado.");rollbackIfPending();return false;}
 Serial.println("[OTA] HMAC verificado.");
 reserved=prefs.getULong64("reserved",0);counter=reserved;
 if(!hmac(reinterpret_cast<const uint8_t*>(Ameri::identity.controlKey.c_str()),Ameri::identity.controlKey.length(),"AMERI-OTA-1",otaKey))return false;
 mux=xSemaphoreCreateMutex();if(!mux){rollbackIfPending();return false;}
 String stored=prefs.getString("context","");JsonDocument d;
 if(stored.length()&&ameriParseObject(d,stored)){jobText=d["job"].as<String>();phase=d["state"].as<String>();progress=d["progress"]|0U;errorCode=d["error"].as<String>();terminalAcked=d["acked"]|false;}
 bootTrusted=inspectBoot();esp_ota_img_states_t stateNow=ESP_OTA_IMG_UNDEFINED;const auto* running=esp_ota_get_running_partition();
 pendingBoot=esp_ota_get_state_partition(running,&stateNow)==ESP_OK&&stateNow==ESP_OTA_IMG_PENDING_VERIFY;
 if(jobText.length()&&!terminal()){
  JsonDocument j;deserializeJson(j,jobText);
  bool target=j["target_address"].as<uint32_t>()==running->address&&j["version"].as<String>()==esp_app_get_description()->version;
  if(target&&pendingBoot){phase="rebooting";}
  else if(target&&stateNow==ESP_OTA_IMG_VALID)state("confirmed",100);
  else{
   const auto* invalid=esp_ota_get_last_invalid_partition();
   if(invalid&&invalid->address==j["target_address"].as<uint32_t>())state("rolled_back",100,"application_rollback");
   else state("failed",progress,"interrupted_before_new_boot");
  }
 }
 if(xTaskCreatePinnedToCore(run,"ameri-ota",20480,nullptr,tskIDLE_PRIORITY,&task,0)!=pdPASS){vSemaphoreDelete(mux);mux=nullptr;rollbackIfPending();return false;}return true;
}
void tick(const Safety& s,const Network& n,bool (*boundary)()){
 if(!mux)return;xSemaphoreTake(mux,portMAX_DELAY);safety=s;network=n;xSemaphoreGive(mux);
 if(boundaryPrepared){boundaryPrepared=false;boundaryDone=true;}
 if(boundaryWanted&&!boundaryDone&&boundary){boundaryWanted=false;boundaryPrepared=boundary();}
}
bool active(){return working||pendingBoot;}
bool ready(){return bootTrusted;}
bool takeMeasurementGap(){bool changed=wroteSinceBoundary;wroteSinceBoundary=false;return changed;}
String diagnostics(){JsonDocument d;d["ota_active"]=active();d["rollback_supported"]=ready();d["ota_supported"]=mux&&bootTrusted&&currentValid&&snapshot().otaEnabled;d["bootloader_verified"]=bootTrusted;d["project"]=AMERI_PROJECT;d["version"]=esp_app_get_description()->version;d["board"]=AMERI_BOARD;d["chip_id"]=AMERI_CHIP_ID;String s;serializeJson(d,s);return s;}
}
