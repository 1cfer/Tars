#include "Tars.h"
#include "AmeriIdentity.h"
#include "AmeriJson.h"
#include "TlsCa.h"
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
namespace Tars {
Config config;Reading reading;Delivery delivery[DESTINATIONS];SemaphoreHandle_t mutex=nullptr;
namespace {
Preferences prefs;String backup;uint64_t probationAt=0;bool controlOk=false,telemetryOk=false;
bool idOK(const String&s){if(!s.length()||s.length()>24)return false;for(char c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;return true;}
bool keyOK(const String&s){if(s.length()!=64)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
void encode(JsonDocument&d,const Config&c,bool secret){
 d["config_version"]=c.version;d["device_id"]=c.id;d["ssid"]=c.ssid;d["read_ms"]=c.readMs;d["send_ms"]=c.sendMs;d["retry_ms"]=c.retryMs;
 d["display_timeout_ms"]=c.displayMs;d["page_ms"]=c.pageMs;d["control_poll_ms"]=c.controlPollMs;d["cpu_mhz"]=c.cpuMHz;d["brightness"]=c.brightness;d["wifi_sleep"]=c.wifiSleep;d["ota_enabled"]=c.otaEnabled;
 d["control_url"]=c.controlUrl;d["ntp_server"]=c.ntpServer;d["ntp_fallback"]=c.ntpFallback;d["temperature_offset"]=c.tempOffset;d["humidity_offset"]=c.humOffset;d["lux_offset"]=c.luxOffset;d["lux_slope"]=c.luxSlope;
 d["noise_slope"]=c.noiseSlope;d["noise_offset"]=c.noiseOffset;d["noise_verified"]=c.noiseVerified;
 if(secret){d["wifi_password"]=c.password;d["control_key"]=c.controlKey;d["ap_password"]=c.apPassword;d["ca_pem"]=c.ca;}
 d["wifi_password_set"]=c.password.length()>0;d["control_key_set"]=c.controlKey.length()>0;
 JsonArray a=d["destinations"].to<JsonArray>();
 for(unsigned i=0;i<DESTINATIONS;++i){const auto&x=c.destinations[i];JsonObject o=a.add<JsonObject>();o["slot"]=i;o["enabled"]=x.enabled;o["url"]=x.url;o["method"]=x.method;o["auth"]=x.auth;o["fiware_service"]=x.service;o["fiware_servicepath"]=x.path;o["token_url"]=x.tokenUrl;o["client_id"]=x.clientId;
  o["bearer_set"]=x.bearer.length()>0;o["oauth_user_set"]=x.user.length()>0;o["oauth_password_set"]=x.password.length()>0;o["client_secret_set"]=x.clientSecret.length()>0;
  if(secret){o["bearer"]=x.bearer;o["oauth_user"]=x.user;o["oauth_password"]=x.password;o["client_secret"]=x.clientSecret;o["ca_pem"]=x.ca;}
 }
}
bool decode(JsonObjectConst d,Config&c,String&error,bool stored=false){
 const char*keys[]={"config_version","expected_config_version","device_id","ssid","wifi_password","control_url","control_key","ap_password","ca_pem","open_wifi","read_ms","send_ms","retry_ms","display_timeout_ms","page_ms","control_poll_ms","cpu_mhz","brightness","wifi_sleep","ota_enabled","noise_verified","temperature_offset","humidity_offset","lux_offset","lux_slope","noise_slope","noise_offset","destinations","wifi_password_set","control_key_set","ntp_server","ntp_fallback"};
 for(JsonPairConst p:d){bool known=false;for(const char*k:keys)known|=String(p.key().c_str())==k;if(!known){error="Parametro desconocido: "+String(p.key().c_str());return false;}}
 auto text=[&](const char*k,String&v,bool confidential=false){if(d.containsKey(k)){if(!d[k].is<String>()){error=String(k)+": tipo invalido";return;}String s=d[k].as<String>();if(!confidential||stored||s.length())v=s;}};
 text("device_id",c.id);text("ssid",c.ssid);text("wifi_password",c.password,true);text("control_url",c.controlUrl);text("control_key",c.controlKey,true);text("ap_password",c.apPassword,true);text("ca_pem",c.ca,true);text("ntp_server",c.ntpServer);text("ntp_fallback",c.ntpFallback);
 if(d.containsKey("open_wifi")){if(!d["open_wifi"].is<bool>())error="open_wifi invalido";else if(d["open_wifi"].as<bool>())c.password="";}
 auto number=[&](const char*k,uint32_t&v,uint32_t lo,uint32_t hi){if(!d.containsKey(k))return;if(!d[k].is<uint32_t>()||d[k].as<uint32_t>()<lo||d[k].as<uint32_t>()>hi)error=String(k)+": fuera de rango";else v=d[k].as<uint32_t>();};
 number("read_ms",c.readMs,1000,3600000);number("send_ms",c.sendMs,5000,86400000);number("retry_ms",c.retryMs,1000,3600000);number("display_timeout_ms",c.displayMs,0,86400000);number("page_ms",c.pageMs,0,60000);number("control_poll_ms",c.controlPollMs,5000,300000);number("cpu_mhz",c.cpuMHz,80,240);number("brightness",c.brightness,1,255);
 if(c.cpuMHz!=80&&c.cpuMHz!=160&&c.cpuMHz!=240)error="cpu_mhz: 80,160 o 240";
 if(c.pageMs&&c.pageMs<2000)error="page_ms: 0 o >=2000";
 if(c.readMs>c.sendMs)error="read_ms debe ser <= send_ms";
 for(const char*k:{"wifi_sleep","noise_verified","ota_enabled"})if(d.containsKey(k)){if(!d[k].is<bool>())error=String(k)+": tipo invalido";else if(String(k)=="wifi_sleep")c.wifiSleep=d[k];else if(String(k)=="ota_enabled")c.otaEnabled=d[k];else c.noiseVerified=d[k];}
 auto real=[&](const char*k,double&v,double lo,double hi){if(!d.containsKey(k))return;if(!d[k].is<double>()||!isfinite(d[k].as<double>())||d[k].as<double>()<lo||d[k].as<double>()>hi)error=String(k)+": invalido";else v=d[k].as<double>();};
 real("temperature_offset",c.tempOffset,-30,30);real("humidity_offset",c.humOffset,-50,50);real("lux_offset",c.luxOffset,-10000,10000);real("lux_slope",c.luxSlope,0.0001,10000);real("noise_slope",c.noiseSlope,0.0001,1000);real("noise_offset",c.noiseOffset,-200,200);
 if(d.containsKey("destinations")){
  if(!d["destinations"].is<JsonArrayConst>()||d["destinations"].size()>DESTINATIONS){error="destinations invalido";return false;}bool seen[DESTINATIONS]={};
  for(JsonObjectConst o:d["destinations"].as<JsonArrayConst>()){
   const char*allowed[]={"slot","enabled","url","method","auth","bearer","token_url","oauth_user","oauth_password","client_id","client_secret","fiware_service","fiware_servicepath","ca_pem","bearer_set","oauth_user_set","oauth_password_set","client_secret_set","reroute_pending"};
   for(JsonPairConst p:o){bool known=false;for(const char*k:allowed)known|=String(p.key().c_str())==k;if(!known){error="Parametro de destino desconocido";return false;}}
   if(o.isNull()||!o["slot"].is<unsigned>()||o["slot"].as<unsigned>()>=DESTINATIONS){error="slot invalido";return false;}unsigned i=o["slot"].as<unsigned>();if(seen[i]){error="slot duplicado";return false;}seen[i]=true;auto&x=c.destinations[i];
   if(o.containsKey("reroute_pending")&&!o["reroute_pending"].is<bool>()){error="reroute_pending invalido";return false;}
   auto str=[&](const char*k,String&v,bool confidential=false){if(o.containsKey(k)){if(!o[k].is<String>()){error=String(k)+": tipo invalido";return;}String s=o[k].as<String>();if(!confidential||stored||s.length())v=s;}};
   if(o.containsKey("enabled")){if(!o["enabled"].is<bool>())error="enabled invalido";else x.enabled=o["enabled"];}
   str("url",x.url);str("method",x.method);str("auth",x.auth);str("bearer",x.bearer,true);str("token_url",x.tokenUrl);str("oauth_user",x.user,true);str("oauth_password",x.password,true);str("client_id",x.clientId);str("client_secret",x.clientSecret,true);str("fiware_service",x.service);str("fiware_servicepath",x.path);str("ca_pem",x.ca,true);
  }
 }
 for(const String&s:{c.ntpServer,c.ntpFallback}){if(!s.length()||s.length()>253)error="Servidor NTP invalido";for(char ch:s)if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='.'||ch=='-'))error="Servidor NTP invalido";}
 if(!idOK(c.id)||c.ssid.length()>32||c.password.length()>63||(c.password.length()&&c.password.length()<8)||c.apPassword.length()<12||c.apPassword.length()>63)error="Identidad o claves WiFi invalidas";
 if(c.controlKey.length()&&!keyOK(c.controlKey))error="Clave HMAC invalida: requiere 64 hex minusculas";
 if(c.controlUrl.length()&&(!validUrl(c.controlUrl,true)||!keyOK(c.controlKey)||c.id=="new"))error="Control requiere ID definitivo, HTTPS y clave HMAC de 64 hex minusculas";
 if(c.ca.indexOf("-----BEGIN CERTIFICATE-----")<0||c.ca.length()>5000)error="CA global invalida";
 for(auto&x:c.destinations){
  if((x.url.length()&&!validUrl(x.url))||(x.enabled&&!x.url.length())||(x.method!="POST"&&x.method!="PATCH")||(x.auth!="none"&&x.auth!="bearer"&&x.auth!="oauth_password")||!validHeader(x.service)||!validHeader(x.path)||!validHeader(x.bearer))error="Destino, metodo o autenticacion invalidos";
  if(x.auth=="bearer"&&!x.bearer.length())error="Falta Bearer del destino";
  if(x.auth=="oauth_password"&&(!validUrl(x.tokenUrl)||!x.user.length()||!x.password.length()||!x.clientId.length()||!x.clientSecret.length()))error="Faltan credenciales Keyrock";
  if(x.ca.length()&&(x.ca.indexOf("-----BEGIN CERTIFICATE-----")<0||x.ca.length()>5000))error="CA del destino invalida";
  if(x.user.length()>256||x.password.length()>256||x.clientId.length()>256||x.clientSecret.length()>256||x.service.length()>64||x.path.length()>128)error="Campo demasiado largo";
 }
 return !error.length();
}
void syncIdentity(){if(Ameri::identity.bound)return;Ameri::identity.id=config.id;Ameri::identity.controlKey=config.controlKey;Ameri::identity.bound=config.id!="new"&&config.controlUrl.length()&&keyOK(config.controlKey);}
bool write(const Config&c){String s=configJson(c,true);return s.length()<=14000&&prefs.putString("config",s)==s.length();}
}
Config snapshot(){Lock lock;return config;}
uint32_t configVersion(){Lock lock;return config.version;}
String configJson(const Config&c,bool secret){JsonDocument d;encode(d,c,secret);String s;serializeJson(d,s);return s;}
bool loadConfig(){
 if(!prefs.begin("tars4-config",false))return false;config.ca=Certificates::ROOT_CA;
 String stored=prefs.getString("config",""),old=prefs.getString("backup","");
 if(old.length()){stored=old;if(prefs.putString("config",old)!=old.length())return false;prefs.remove("backup");}
 if(stored.length()){JsonDocument d;String error;if(!ameriParseObject(d,stored)||!decode(d.as<JsonObjectConst>(),config,error,true))return false;config.version=d["config_version"]|1U;}
 else {
  config.apPassword="tars12345678";
  // Solo migra valores realmente guardados: nunca recupera secretos compilados del ZIP.
  Preferences p;if(p.begin("appconfig",true)){
   config.id=p.getString("hostname","new");config.destinations[0].url=p.getString("serverUrl","");config.destinations[0].method="PATCH";config.destinations[0].enabled=validUrl(config.destinations[0].url);
   bool skip=p.getBool("skipToken",true);auto&x=config.destinations[0];x.tokenUrl=p.getString("tokenUrl","");x.user=p.getString("keyrockUser","");x.password=p.getString("keyrockPass","");x.clientId=p.getString("clientId","");x.clientSecret=p.getString("clientSecret","");
   if(!skip&&x.tokenUrl.length()&&x.user.length()&&x.password.length()&&x.clientId.length()&&x.clientSecret.length())x.auth="oauth_password";
   else if(!skip)x.enabled=false; // Nunca omite autenticacion exigida por una configuracion anterior.
   config.readMs=p.getULong("intervaloLectura",config.readMs);config.sendMs=p.getULong("intervaloEnvio",config.sendMs);p.end();
  }
  if(p.begin("agevital",true)){config.ssid=p.getString("lastSSID","");config.password=p.getString("lastPASS","");p.end();}
  String error;JsonDocument d;encode(d,config,true);if(!decode(d.as<JsonObjectConst>(),config,error,true))return false;
  if(!write(config))return false;
 }
 // No restablecer claves durante OTA: configuraciones existentes conservan su AP privado.
 if(!prefs.getBool("ap-standard-v2",false)&&!prefs.putBool("ap-standard-v2",true))return false;
 syncIdentity();return true;
}
bool applyConfig(const String&json,String&error){
 Lock lock;JsonDocument d;if(!ameriParseObject(d,json)||json.length()>14000){error="JSON invalido";return false;}
 if(!d["expected_config_version"].is<uint32_t>()||d["expected_config_version"].as<uint32_t>()!=config.version){error="version_conflict";return false;}
 if(probationAt){error="network_probation";return false;}
 Config next=config;if(!decode(d.as<JsonObjectConst>(),next,error))return false;if(configJson(next,false).length()>5000){error="Configuracion publica demasiado grande";return false;}
 if(config.id!="new"&&next.id!=config.id){error="ID ya vinculado; no se renombra por OTA";return false;}
 if(next.controlKey!=config.controlKey&&config.controlKey.length()){error="HMAC ya vinculada; no se cambia remotamente";return false;}
 for(unsigned i=0;i<DESTINATIONS;++i)if(delivery[i].pending&&next.destinations[i].url!=config.destinations[i].url){bool reroute=false;for(JsonObjectConst o:d["destinations"].as<JsonArrayConst>())if(o["slot"].as<unsigned>()==i&&o["reroute_pending"].is<bool>())reroute=o["reroute_pending"].as<bool>();if(!reroute){error="pending_destination_records: autorice reroute_pending para enviar pendientes a la nueva URL";return false;}}
 bool network=next.ssid!=config.ssid||next.password!=config.password||next.controlUrl!=config.controlUrl||next.ca!=config.ca;
 if(config.version==UINT32_MAX){error="version_exhausted";return false;}next.version=config.version+1;
 Config restored=config; // Una prueba de red no deshace la primera vinculacion de identidad.
 if(restored.id=="new"&&next.id!="new")restored.id=next.id;
 if(!restored.controlKey.length()&&next.controlKey.length())restored.controlKey=next.controlKey;
 String old=configJson(restored,true);
 if(network&&(prefs.putString("backup",old)!=old.length())){error="backup_failed";return false;}
 if(!write(next)){if(network)prefs.remove("backup");error="storage_failed";return false;}
 config=next;syncIdentity();
 if(network){backup=old;probationAt=nowUs()+180000000;controlOk=!config.controlUrl.length();telemetryOk=false;bool enabled=false;for(auto&x:config.destinations)enabled|=x.enabled;if(!enabled)telemetryOk=true;}
 return true;
}
bool commitPairing(const String&payload,bool history,String&error){
 Lock lock;if(probationAt){error="network_probation";return false;}
 if(config.controlKey.length()){error="already_bound";return false;}
 JsonDocument p;if(!ameriParseObject(p,payload)){error="invalid_payload";return false;}
 Config next=config;String id=p["device_id"].as<String>();
 if(config.id!="new"&&config.id!=id){error="identity_mismatch";return false;}
 if(p["physical_id"].as<String>()!=WiFi.macAddress()){error="physical_mismatch";return false;}
 next.id=id;next.controlUrl=p["control_url"].as<String>();next.controlKey=p["control_key"].as<String>();next.apPassword=p["ap_password"].as<String>();
 if(config.version==UINT32_MAX){error="version_exhausted";return false;}next.version=config.version+1;
 String note="saved_waiting_signed_contact";
 if(history){
  String url=p["history_url"].as<String>();int free=-1;bool exists=false;
  for(unsigned i=0;i<DESTINATIONS;++i){const auto&x=next.destinations[i];exists|=url.length()&&x.url==url;
   if(free<0&&!x.enabled&&!x.url.length()&&!delivery[i].pending&&x.auth=="none"&&!x.bearer.length()&&!x.tokenUrl.length()&&!x.user.length()&&!x.password.length()&&!x.clientId.length()&&!x.clientSecret.length()&&!x.ca.length()&&!x.service.length()&&x.path=="/"&&x.method=="POST")free=i;
  }
  if(!url.length())note="saved_history_not_available";
  else if(exists)note="saved_history_already_present";
  else if(free<0)note="saved_history_requires_free_slot";
  else {auto&x=next.destinations[free];x.enabled=true;x.url=url;x.method="POST";x.auth="bearer";x.bearer=p["history_bearer"].as<String>();note="saved_history_added_review_orion_duplication";}
 }
 JsonDocument validated;encode(validated,next,true);if(!decode(validated.as<JsonObjectConst>(),next,error,true))return false;
 // Preparar sidecar antes del único commit: solo es efectivo si coincide con la configuración confirmada.
 if(!storeOtaBinding(next,p["ota_base_url"].as<String>(),error))return false;
 if(!write(next)||prefs.getString("config","")!=configJson(next,true)){error="storage_failed";return false;}
 config=next;syncIdentity();error=note;return true;
}
bool configProbation(){Lock lock;return probationAt!=0;}
void networkSuccess(bool control,unsigned slot,uint32_t version){Lock lock;if(!probationAt||version!=config.version)return;unsigned primary=0;while(primary<DESTINATIONS&&!config.destinations[primary].enabled)++primary;if(control)controlOk=true;else if(slot==primary)telemetryOk=true;if(controlOk&&telemetryOk){prefs.remove("backup");backup="";probationAt=0;}}
void configTick(){Lock lock;if(probationAt&&controlOk&&telemetryOk&&(!config.ssid.length()||WiFi.status()==WL_CONNECTED)){prefs.remove("backup");backup="";probationAt=0;}if(probationAt&&nowUs()>=probationAt){JsonDocument d;Config old;String e;if(ameriParseObject(d,backup)&&decode(d.as<JsonObjectConst>(),old,e,true)){old.version=d["config_version"]|1U;if(write(old)){config=old;syncIdentity();prefs.remove("backup");backup="";probationAt=0;}}}}
}
