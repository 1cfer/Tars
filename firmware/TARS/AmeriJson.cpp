#include "AmeriJson.h"
#include <vector>
namespace {
struct Scanner{
 const String& s;size_t p=0;
 explicit Scanner(const String& text):s(text){}
 void ws(){while(p<s.length()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;}
 bool str(String* key=nullptr){size_t begin=p;if(p>=s.length()||s[p++]!='"')return false;
  while(p<s.length()){char c=s[p++];if(c=='\\'){if(p>=s.length())return false;++p;}else if(c=='"'){
   if(key){JsonDocument d;if(deserializeJson(d,s.substring(begin,p)))return false;*key=d.as<String>();}return true;
  }else if(uint8_t(c)<32)return false;}return false;
 }
 bool value(int depth){if(depth>16)return false;ws();if(p>=s.length())return false;
  char c=s[p];if(c=='"')return str();
  if(c=='{'||c=='['){bool object=c=='{';char end=object?'}':']';++p;ws();if(p<s.length()&&s[p]==end){++p;return true;}
   std::vector<String> keys;int count=0;
   for(;;){ws();if(++count>256)return false;if(object){String k;if(!str(&k))return false;for(auto& old:keys)if(old==k)return false;keys.push_back(k);ws();if(p>=s.length()||s[p++]!=':')return false;}
    if(!value(depth+1))return false;ws();if(p>=s.length())return false;char next=s[p++];if(next==end)return true;if(next!=',')return false;
   }
  }
  size_t start=p;while(p<s.length()&&s[p]!=','&&s[p]!=']'&&s[p]!='}'&&s[p]!=' '&&s[p]!='\n'&&s[p]!='\r'&&s[p]!='\t')++p;
  String t=s.substring(start,p);if(t=="true"||t=="false"||t=="null")return true;
  size_t i=0;if(t[i]=='-')++i;if(i>=t.length())return false;
  if(t[i]=='0')++i;else{if(t[i]<'1'||t[i]>'9')return false;while(i<t.length()&&t[i]>='0'&&t[i]<='9')++i;}
  if(i<t.length()&&t[i]=='.'){size_t digits=++i;while(i<t.length()&&t[i]>='0'&&t[i]<='9')++i;if(i==digits)return false;}
  if(i<t.length()&&(t[i]=='e'||t[i]=='E')){++i;if(t[i]=='+'||t[i]=='-')++i;size_t digits=i;while(i<t.length()&&t[i]>='0'&&t[i]<='9')++i;if(i==digits)return false;}
  return i==t.length();
 }
};
}
bool ameriParseObject(JsonDocument& d,const String& s){
 if(!s.length()||s.length()>24576)return false;Scanner scan{s};if(!scan.value(0))return false;scan.ws();
 return scan.p==s.length()&&!deserializeJson(d,s)&&d.is<JsonObject>();
}
