#include "Tars.h"
#include <WiFi.h>
#include <time.h>
namespace Tars {
String iso(int64_t utc){if(!utc)return "";tm t;time_t x=utc;gmtime_r(&x,&t);char b[25];strftime(b,sizeof b,"%Y-%m-%dT%H:%M:%SZ",&t);return b;}
String buildPayload(const Config&c,const Reading&r,uint32_t seq,const String&boot,uint64_t start,uint32_t samples,const uint32_t*counts){
 JsonDocument d;bool soundVerified=c.noiseVerified||(c.noiseSlope==50&&c.noiseOffset==0);
 auto num=[&](const char*k,double v,const char*unit){JsonObject o=d[k].to<JsonObject>();o["type"]="Number";if(isfinite(v))o["value"]=v;else o["value"]=nullptr;if(unit){auto m=o["metadata"]["unitCode"].to<JsonObject>();m["type"]="Text";m["value"]=unit;}};
 auto text=[&](const char*k,const String&v){auto o=d[k].to<JsonObject>();o["type"]="Text";o["value"]=v;};
 num("temperature",r.temp,"CEL");num("humidity",r.hum,"P1");num("illuminance",r.lux,"LUX");num("noise_voltage",r.voltage,"VLT");num("noise_estimate_db",r.noise,nullptr);
 num("noise",soundVerified?r.noise:NAN,"2N");text("noise_metric","A_weighted_Fast_sample_mean;not_Leq");
 {auto m=d["noise"]["metadata"].as<JsonObject>();auto w=m["frequencyWeighting"].to<JsonObject>();w["type"]="Text";w["value"]="A";auto model=m["sensorModel"].to<JsonObject>();model["type"]="Text";model["value"]="DFRobot_SEN0232";auto cal=m["calibrationMethod"].to<JsonObject>();cal["type"]="Text";cal["value"]=c.noiseVerified?"field_reference_verified":(c.noiseSlope==50&&c.noiseOffset==0?"manufacturer_transfer":"configured_adjustment");auto t=m["sensorTimeConstantMs"].to<JsonObject>();t["type"]="Number";t["value"]=125;}
 text("boot_id",boot);num("sequence",seq,nullptr);text("device_id",c.id);text("physical_id",WiFi.macAddress());text("firmware_version",AMERI_VERSION);num("schema_version",1,nullptr);
 {auto o=d["observedAt"].to<JsonObject>();o["type"]="DateTime";if(r.utc)o["value"]=iso(r.utc);else o["value"]=nullptr;}
 num("interval_seconds",(r.uptimeUs-start)/1e6,"SEC");num("interval_start_uptime_us",start,nullptr);num("interval_end_uptime_us",r.uptimeUs,nullptr);num("sample_count",samples,nullptr);num("invalid_measurements",r.failures,nullptr);num("config_version",c.version,nullptr);
 num("uptime_seconds",r.uptimeUs/1e6,"SEC");if(WiFi.status()==WL_CONNECTED)num("wifi_rssi",WiFi.RSSI(),nullptr);
 auto flags=d["quality_flags"].to<JsonObject>();flags["type"]="StructuredValue";auto f=flags["value"].to<JsonObject>();f["clock_ready"]=r.utc!=0;f["noise_verified"]=soundVerified;f["noise_field_verified"]=c.noiseVerified;f["noise_verification_basis"]=c.noiseVerified?"field_reference_declared":(soundVerified?"manufacturer_factory_transfer":"unverified_configured_adjustment");f["sensor_invalid_mask"]=r.failures;f["aggregation"]="mean_of_valid_samples";
 auto valid=f["valid_samples"].to<JsonObject>();const char*names[]={"temperature","humidity","illuminance","noise_voltage","noise_estimate_db"};for(unsigned i=0;i<5;++i)valid[names[i]]=counts[i];valid["noise"]=soundVerified?counts[4]:0;
 String out;serializeJson(d,out);return out;
}
String manifest(){JsonDocument d;d["manifest_version"]=1;d["family"]=AMERI_FAMILY;d["board"]=AMERI_BOARD;d["project"]=AMERI_PROJECT;d["firmware_version"]=AMERI_VERSION;d["chip_id"]=AMERI_CHIP_ID;d["telemetry_format"]="NGSIv2-attributes";d["control_protocol"]=2;d["ota_protocol"]=1;d["max_destinations"]=DESTINATIONS;
 auto a=d["variables"].to<JsonArray>();for(const char*n:{"temperature","humidity","illuminance","noise_voltage","noise_estimate_db","noise","interval_seconds","sample_count","wifi_rssi","uptime_seconds","quality_flags"})a.add(n);d["capabilities"]["maintenance"]=true;d["capabilities"]["remote_config"]=true;d["capabilities"]["actuator"]=false;String out;serializeJson(d,out);return out;}
}
