#include "Tars.h"
#include "AmeriJson.h"
#include "AmeriOta.h"
#include <Preferences.h>
#include <WiFi.h>
#include <mbedtls/md.h>
#include <time.h>
namespace Tars {
int request(const Config&,const String&,const String&,const String&,const Destination*,String&,const String&,const String&);
namespace {
Preferences journal;bool ready=false;uint64_t last=0;String ack,ackUrl,ackCa;
String hmac(const String&key,const String&s){uint8_t b[32];if(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),reinterpret_cast<const uint8_t*>(key.c_str()),key.length(),reinterpret_cast<const uint8_t*>(s.c_str()),s.length(),b)!=0)return "";return hex(b,32);}
bool save(){if(persistenceFault())return false;JsonDocument d;d["sequence"]=last;d["ack"]=ack;d["url"]=ack.length()?ackUrl:"";d["ca"]=ack.length()?ackCa:"";String s;serializeJson(d,s);return journal.putString("journal",s)==s.length();}
String makeAck(const Config&c,const String&id,uint64_t seq,bool success,const String&reason){JsonDocument d;d["protocol_version"]=2;d["device_id"]=c.id;d["command_id"]=id;d["sequence"]=seq;d["status"]=success?"executed":"failed";d["maintenance_active"]=maintenanceActive();d["remaining_seconds"]=maintenanceRemaining();d["nonce"]=randomHex(24);
 JsonDocument result;result["reason"]=reason;result["config_version"]=snapshot().version;String r;serializeJson(result,r);d["result_json"]=r;
 if(maintenanceActive()){d["ap_ssid"]="TARS-"+c.id;d["ap_url"]="http://192.168.4.1";}
 d["device_signature"]=hmac(c.controlKey,String("v2-ack\n")+c.id+"\n"+id+"\n"+String((unsigned long long)seq)+"\n"+(success?"executed":"failed")+"\n"+(d["maintenance_active"].as<bool>()?"1":"0")+"\n"+String(d["remaining_seconds"].as<uint32_t>())+"\n"+d["nonce"].as<String>()+"\n"+sha(r));String s;serializeJson(d,s);return s;
}
}
void controlTick(const Config&c){
 if(!ready){if(!journal.begin("tars4-control",false))return;String stored=journal.getString("journal","");if(stored.length()){JsonDocument d;if(!ameriParseObject(d,stored))return;last=d["sequence"]|uint64_t(0);ack=d["ack"].as<String>();ackUrl=d["url"].as<String>();ackCa=d["ca"].as<String>();}ready=true;if(!ack.length()&&ackCa.length()&&!save()){pausePersistence("control_journal_failed");return;}}
 String response;
 if(ack.length()&&!persistenceFault()){Config previous=c;previous.ca=ackCa;int code=request(previous,ackUrl,"POST",ack,nullptr,response,"application/json","");if(code<200||code>=300)return;String old=ack;ack="";if(!save()){ack=old;pausePersistence("control_journal_failed");return;}}
 if(!c.controlUrl.length()||c.controlKey.length()!=64)return;
 JsonDocument status;bool active=maintenanceActive();uint32_t remain=maintenanceRemaining();String nonce=randomHex(24),s=diagnostics();status["protocol_version"]=2;status["firmware_protocol"]=2;status["device_id"]=c.id;status["last_sequence"]=last;status["maintenance_active"]=active;status["remaining_seconds"]=remain;status["nonce"]=nonce;status["clock_ready"]=clockReady();status["status_json"]=s;status["sta_ip"]=WiFi.localIP().toString();
 status["device_signature"]=hmac(c.controlKey,String("v2-status\n")+c.id+"\n"+String((unsigned long long)last)+"\n"+(active?"1":"0")+"\n"+String(remain)+"\n"+nonce+"\n"+sha(s));String body;serializeJson(status,body);
 String base=c.controlUrl+"/api/devices/"+c.id+"/commands";
 if(request(c,base+"/next","POST",body,nullptr,response,"application/json","")!=200)return;networkSuccess(true,0,c.version);
 JsonDocument d;if(!ameriParseObject(d,response))return;if(d.containsKey("command"))pairingControlSeen(c);JsonObjectConst cmd=d["command"].as<JsonObjectConst>();if(cmd.isNull())return;
 if(!cmd["protocol_version"].is<unsigned>()||cmd["protocol_version"].as<unsigned>()!=2||!cmd["sequence"].is<uint64_t>()||!cmd["expires_at"].is<int64_t>()||!cmd["duration_seconds"].is<uint32_t>()||!cmd["config_json"].is<String>())return;
 String device=cmd["device_id"].as<String>(),id=cmd["command_id"].as<String>(),action=cmd["action"].as<String>(),patch=cmd["config_json"].as<String>();uint64_t seq=cmd["sequence"].as<uint64_t>();int64_t expiry=cmd["expires_at"].as<int64_t>();uint32_t seconds=cmd["duration_seconds"].as<uint32_t>();
 if(device!=c.id||!id.length()||id.length()>64||patch.length()>8192||seq<=last||seq>9007199254740991ULL||!clockReady()||expiry<=time(nullptr)||expiry>time(nullptr)+120)return;
 for(char ch:id)if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'))return;
 bool actionOK=(action=="open_maintenance"&&seconds>=60&&seconds<=900)||((action=="close_maintenance"||action=="update_config")&&seconds==0);if(!actionOK)return;
 String canonical="v2\n"+device+"\n"+id+"\n"+String((unsigned long long)seq)+"\n"+action+"\n"+String((long long)expiry)+"\n"+String(seconds)+"\n"+sha(patch);
 if(!equal(hmac(c.controlKey,canonical),cmd["signature"].as<String>()))return;
 if(persistenceFault())return;
 // Reserva secuencia y ACK de fallo en UN documento antes de cualquier efecto.
 last=seq;ackUrl=base+"/ack";ackCa=c.ca;ack=makeAck(c,id,seq,false,"interrupted_or_storage_failed");if(!save()){ready=false;pausePersistence("control_journal_failed");return;}
 bool ok=false;String reason;
 if(AmeriOta::active()||localUpload)reason="ota_busy";
 else if(action=="update_config")ok=applyConfig(patch,reason);
 else {requestMaintenance(action=="open_maintenance"?seconds:UINT32_MAX);uint64_t deadline=nowUs()+2000000;while(requestedMaintenance&&nowUs()<deadline)vTaskDelay(pdMS_TO_TICKS(20));ok=!requestedMaintenance&&(maintenanceActive()==(action=="open_maintenance"));if(!ok)reason="maintenance_unavailable";}
 String final=makeAck(c,id,seq,ok,reason),old=ack;ack=final;if(!save()){ack=old;pausePersistence("control_journal_failed");}
}
}
