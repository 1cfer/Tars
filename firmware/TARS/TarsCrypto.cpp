#include "Tars.h"
#include <esp_timer.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <time.h>
namespace Tars {
uint64_t nowUs(){return esp_timer_get_time();}
String hex(const uint8_t* p,size_t n){String s;const char*h="0123456789abcdef";s.reserve(n*2);for(size_t i=0;i<n;++i){s+=h[p[i]>>4];s+=h[p[i]&15];}return s;}
String randomHex(size_t n){uint8_t b[32];if(n>32)return "";esp_fill_random(b,n);return hex(b,n);}
String sha(const String& s){uint8_t b[32];mbedtls_sha256(reinterpret_cast<const uint8_t*>(s.c_str()),s.length(),b,0);return hex(b,32);}
bool equal(const String&a,const String&b){if(a.length()!=b.length())return false;uint8_t d=0;for(size_t i=0;i<a.length();++i)d|=a[i]^b[i];return d==0;}
bool clockReady(){return time(nullptr)>=1704067200;}
bool validHeader(const String&s){if(s.length()>2048)return false;for(char c:s)if(c<32||c==127)return false;return true;}
bool validUrl(const String&s,bool base){
 if(!s.startsWith("https://")||s.length()>512||s.indexOf('@')>=0||s.indexOf('#')>=0)return false;
 for(char c:s)if(c<=32||c>=127||c=='\\')return false;
 int end=s.indexOf('/',8);String host=end<0?s.substring(8):s.substring(8,end);
 if(!host.length()||host.indexOf('?')>=0)return false;
 if(base&&(end>=0||s.indexOf('?')>=0))return false;
 return true;
}
}
