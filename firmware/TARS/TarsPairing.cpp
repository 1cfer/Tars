#include "Tars.h"
#include "AmeriJson.h"
#include <Preferences.h>
#include <WiFi.h>
#include <mbedtls/md.h>
#include <time.h>
namespace Tars {
int request(const Config&,const String&,const String&,const String&,const Destination*,String&,const String&,const String&);
namespace {
Preferences pairing;bool ready=false;String journal,sidecar,state="idle",result="",boundId;uint64_t nextAttempt=0,bootStarted=0;int lastHttp=0;
bool lowerHex(const String&s,size_t n){if(s.length()!=n)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
bool deviceId(const String&s){if(!s.length()||s.length()>24||s=="new")return false;for(char c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;return true;}
String anchor(const Config&c){return sha(c.id+"\n"+WiFi.macAddress()+"\n"+c.controlUrl+"\n"+c.controlKey);}
String sign(const String&code,const String&id,const String&nonce,const String&payload){String canonical="ameri-pairing-1\n"+id+"\n"+nonce+"\n"+sha(payload);uint8_t bytes[32];if(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),reinterpret_cast<const uint8_t*>(code.c_str()),code.length(),reinterpret_cast<const uint8_t*>(canonical.c_str()),canonical.length(),bytes)!=0)return "";return hex(bytes,32);}
bool saveJournal(JsonDocument&d){String raw;serializeJson(d,raw);if(raw.length()>4096||pairing.putString("pending",raw)!=raw.length())return false;journal=raw;return true;}
bool purge(){if(!journal.length())return true;if(!pairing.remove("pending"))return false;journal="";return true;}
void fail(const String&why){Lock lock;result=why;state=purge()?"failed":"cleanup_required";}
bool isCommitted(JsonDocument&d,const Config&c){return c.controlKey.length()&&d["commit_anchor"].is<String>()&&equal(d["commit_anchor"].as<String>(),anchor(c));}
bool verifyResponse(const String&raw,JsonDocument&claim,String&payload){
 if(raw.length()>8192)return false;JsonDocument envelope,p;
 if(!ameriParseObject(envelope,raw)||envelope.size()!=5||!envelope["protocol"].is<unsigned>()||envelope["protocol"].as<unsigned>()!=1||!envelope["device_id"].is<String>()||!envelope["nonce"].is<String>()||!envelope["payload_json"].is<String>()||!envelope["signature"].is<String>())return false;
 JsonDocument req;if(!ameriParseObject(req,claim["request_json"].as<String>()))return false;
 String id=req["device_id"].as<String>(),nonce=req["nonce"].as<String>(),code=req["pairing_code"].as<String>();
 if(envelope["device_id"].as<String>()!=id||envelope["nonce"].as<String>()!=nonce||!lowerHex(envelope["signature"].as<String>(),64))return false;
 payload=envelope["payload_json"].as<String>();if(!payload.length()||payload.length()>6144||!equal(sign(code,id,nonce,payload),envelope["signature"].as<String>())||!ameriParseObject(p,payload))return false;
 const char*strings[]={"device_id","physical_id","family","board","project","control_url","ota_base_url","control_key","ap_password","history_url","history_bearer"};
 for(const char*k:strings)if(!p[k].is<String>())return false;
 const char*allowed[]={"device_id","physical_id","family","board","project","chip_id","control_url","ota_base_url","control_key","ap_password","history_url","history_bearer","preserve_local_configuration"};
 for(JsonPairConst q:p.as<JsonObjectConst>()){bool ok=false;for(const char*k:allowed)ok|=String(q.key().c_str())==k;if(!ok)return false;}
 if(p["device_id"].as<String>()!=id||p["physical_id"].as<String>()!=req["physical_id"].as<String>()||p["physical_id"].as<String>()!=WiFi.macAddress()||p["family"].as<String>()!=AMERI_FAMILY||p["board"].as<String>()!=AMERI_BOARD||p["project"].as<String>()!=AMERI_PROJECT||!p["chip_id"].is<unsigned>()||p["chip_id"].as<unsigned>()!=AMERI_CHIP_ID||!p["preserve_local_configuration"].is<bool>()||!p["preserve_local_configuration"].as<bool>())return false;
 String control=p["control_url"].as<String>(),ota=p["ota_base_url"].as<String>(),pass=p["ap_password"].as<String>(),url=p["history_url"].as<String>(),bearer=p["history_bearer"].as<String>();
 if(control!=claim["base"].as<String>()||!validUrl(control,true)||!validUrl(ota,true)||!lowerHex(p["control_key"].as<String>(),64)||pass.length()<12||pass.length()>63||!validHeader(pass))return false;
 if(url.length()?(!validUrl(url)||!bearer.length()||!validHeader(bearer)):(bearer.length()!=0))return false;
 return true;
}
}
void beginPairing(){Lock lock;if(!pairing.begin("tars-pair-v1",false))return;ready=true;bootStarted=nowUs();journal=pairing.getString("pending","");sidecar=pairing.getString("ota_binding","");
 if(journal.length()){JsonDocument d;if(!ameriParseObject(d,journal)){state="failed";result="invalid_pending_state";purge();return;}
  if(isCommitted(d,config)){boundId=config.id;state=purge()?"saved":"cleanup_required";result="saved_waiting_signed_contact";}
  else state="pending";
 }else if(config.controlKey.length()){state="configured";boundId=config.id;}
}
String otaBase(const Config&c){Lock lock;JsonDocument d;if(sidecar.length()&&ameriParseObject(d,sidecar)&&d["base"].is<String>()&&d["anchor"].is<String>()&&equal(d["anchor"].as<String>(),anchor(c))&&validUrl(d["base"].as<String>(),true))return d["base"].as<String>();return c.controlUrl;}
bool storeOtaBinding(const Config&c,const String&base,String&error){Lock lock;if(!ready||!validUrl(base,true)){error="pairing_storage_unavailable";return false;}
 JsonDocument d;if(!ameriParseObject(d,journal)){error="pairing_pending_missing";return false;}d["commit_anchor"]=anchor(c);if(!saveJournal(d)){error="pairing_prepare_failed";return false;}
 JsonDocument extension;extension["base"]=base;extension["anchor"]=anchor(c);String raw;serializeJson(extension,raw);
 if(pairing.putString("ota_binding",raw)!=raw.length()){error="ota_binding_storage_failed";return false;}sidecar=raw;return true;
}
bool startPairing(const String&body,String&error){Lock lock;if(!ready){error="pairing_storage_unavailable";return false;}if(body.length()>1536){error="request_too_large";return false;}
 JsonDocument p;if(!ameriParseObject(p,body)||p.size()!=5||!p["device_id"].is<String>()||!p["control_base_url"].is<String>()||!p["pairing_code"].is<String>()||!p["add_history"].is<bool>()||!p["expected_config_version"].is<uint32_t>()){error="invalid_pairing_fields";return false;}
 if(config.controlKey.length()){error="already_bound_use_update_config";return false;}
 if(p["expected_config_version"].as<uint32_t>()!=config.version){error="version_conflict_reload";return false;}
 if(configProbation()){error="network_probation_wait";return false;}
 String id=p["device_id"].as<String>(),base=p["control_base_url"].as<String>(),code=p["pairing_code"].as<String>();
 if(!deviceId(id)||!validUrl(base,true)||!lowerHex(code,32)||(config.id!="new"&&config.id!=id)){error="invalid_identity_base_or_code";return false;}
 if(!clockReady()||WiFi.status()!=WL_CONNECTED){error="save_wifi_first_wait_for_utc";return false;}
 if(journal.length()){
  JsonDocument prior,req;if(!ameriParseObject(prior,journal)||!ameriParseObject(req,prior["request_json"].as<String>())){error="pending_state_invalid";return false;}
  if(req["device_id"].as<String>()!=id||req["pairing_code"].as<String>()!=code||prior["base"].as<String>()!=base||prior["add_history"].as<bool>()!=p["add_history"].as<bool>()){error="pending_request_retrying_wait_for_expiry";return false;}
  error="";return true; // Misma peticion persistida: conservar nonce/MAC y bytes.
 }
 JsonDocument req;req["protocol"]=1;req["device_id"]=id;req["pairing_code"]=code;req["nonce"]=randomHex(16);req["physical_id"]=WiFi.macAddress();req["family"]=AMERI_FAMILY;req["board"]=AMERI_BOARD;req["project"]=AMERI_PROJECT;req["chip_id"]=AMERI_CHIP_ID;
 String raw;serializeJson(req,raw);JsonDocument record;record["request_json"]=raw;record["base"]=base;record["add_history"]=p["add_history"].as<bool>();record["created_at"]=int64_t(time(nullptr));record["first_attempt_at"]=int64_t(0);
 if(!saveJournal(record)){error="pairing_request_not_persisted";return false;}bootStarted=nowUs();nextAttempt=0;lastHttp=0;state="pending";result="request_saved";return true;
}
void pairingTick(const Config&c){String saved;{Lock lock;if(!ready||!journal.length())return;saved=journal;}
 JsonDocument record;if(!ameriParseObject(record,saved)){fail("invalid_pending_state");return;}
 if(isCommitted(record,c)){Lock lock;boundId=c.id;state=purge()?"saved":"cleanup_required";return;}
 if(c.controlKey.length()){fail("already_bound_use_update_config");return;}
 if((clockReady()&&(time(nullptr)>=record["created_at"].as<int64_t>()+600||(record["first_attempt_at"].as<int64_t>()&&time(nullptr)>=record["first_attempt_at"].as<int64_t>()+180)))||nowUs()-bootStarted>=600000000ULL){fail("expired_request_generate_new_code");return;}
 if(WiFi.status()!=WL_CONNECTED||!clockReady()){Lock lock;state="waiting_network_or_utc";return;}
 if(nowUs()<nextAttempt)return;
 if(!record["first_attempt_at"].as<int64_t>()){record["first_attempt_at"]=int64_t(time(nullptr));Lock lock;if(journal!=saved||!saveJournal(record))return;saved=journal;}
 {Lock lock;state="claiming";nextAttempt=nowUs()+uint64_t(min(c.retryMs,uint32_t(60000)))*1000;}
 JsonDocument req;if(!ameriParseObject(req,record["request_json"].as<String>())){fail("invalid_pending_request");return;}
 String response;int code=request(c,record["base"].as<String>()+"/api/pairing/devices/"+req["device_id"].as<String>()+"/claim","POST",record["request_json"].as<String>(),nullptr,response,"application/json","");
 {Lock lock;lastHttp=code;if(journal!=saved)return;}
 if(code==200){String payload;if(!verifyResponse(response,record,payload)){fail("invalid_signed_pairing_response");return;}
  String note;if(!commitPairing(payload,record["add_history"].as<bool>(),note)){Lock lock;state="retrying";result=note;return;}
  Lock lock;boundId=config.id;result=note;state=purge()?"saved":"cleanup_required";
 }else if(code==400||code==401||code==403||code==404||code==409||code==410||code==422)fail(code==409?"identity_or_nonce_conflict_check_devices":code==403?"invalid_or_expired_code":"pairing_rejected_check_server");
 else {Lock lock;state="retrying";result="network_or_server_error_same_request_retained";}
}
void pairingControlSeen(const Config&c){Lock lock;if(c.id!=config.id||c.controlKey!=config.controlKey)return;if(state=="saved"||state=="configured"||state=="cleanup_required"){state="signed_report_accepted";boundId=c.id;}}
String pairingStatus(){Lock lock;JsonDocument d;d["state"]=state;d["result"]=result;d["http_status"]=lastHttp;d["device_id"]=boundId.length()?boundId:config.id;d["credentials_saved"]=config.controlKey.length()!=0;d["ota_base_url"]=otaBase(config);d["signed_report_accepted"]=state=="signed_report_accepted";d["wifi_connected"]=WiFi.status()==WL_CONNECTED;d["clock_ready"]=clockReady();d["pending"]=journal.length()!=0;String s;serializeJson(d,s);return s;}
}
