#include "Tars.h"
#include "AmeriOta.h"
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <time.h>
namespace Tars {
namespace {
volatile bool boundaryRequested=false,boundaryDone=false;
Adafruit_SSD1306 oled(128,64,&Wire,-1,50000,50000);bool oledOK=false,on=true,hdcOK=false;uint8_t oledAddress=0x3c;uint32_t sensorCpuMHz=0;
unsigned page=0;uint64_t interaction=0,lastFrame=0,lastPage=0;uint8_t brightness=255;
bool pressed=false,raw=false,longHandled=false,recoveryHandled=false;uint64_t debounce=0,pressedAt=0;
float luxHistory[15];unsigned luxCount=0,luxIndex=0;String boot;uint32_t sequence=0;
double sum[5]={};uint32_t counts[5]={},samples=0;uint64_t intervalStart=0;
bool reg(uint8_t address,uint8_t r,uint8_t* out,size_t n){Wire.beginTransmission(address);Wire.write(r);if(Wire.endTransmission(false)!=0)return false;size_t got=Wire.requestFrom(address,uint8_t(n));if(got!=n){while(Wire.available())Wire.read();return false;}for(size_t i=0;i<n;++i)out[i]=Wire.read();return true;}
float hdc(uint8_t r,bool temp){Wire.beginTransmission(0x40);Wire.write(r);if(Wire.endTransmission()!=0)return NAN;vTaskDelay(pdMS_TO_TICKS(20));if(Wire.requestFrom(uint8_t(0x40),uint8_t(2))!=2)return NAN;uint16_t raw=(Wire.read()<<8)|Wire.read();return temp?raw*(165.0f/65536.0f)-40:raw*(100.0f/65536.0f);}
void checkHdc(){uint8_t b[2];hdcOK=reg(0x40,0xfe,b,2)&&b[0]==0x54&&b[1]==0x49&&reg(0x40,0xff,b,2)&&b[0]==0x10&&b[1]==0x50;}
// SEN0232: salida 0,6-2,6 V = 30-130 dBA. No aplicar VREF=5 al ADC ESP32.
float soundDbA(float voltage,double slope,double offset){
 if(!isfinite(voltage)||voltage<0.6f||voltage>2.6f)return NAN;
 double value=voltage*slope+offset;
 return isfinite(value)&&value>=30.0&&value<=130.0?float(value):NAN;
}
void readSensors(const Config&c){
 Reading r;r.uptimeUs=nowUs();r.utc=clockReady()?time(nullptr):0;
 if(!hdcOK)checkHdc();if(hdcOK){float t=hdc(0,true),h=hdc(1,false);if(isfinite(t)&&t>=-40&&t<=125)r.temp=t+c.tempOffset;else {r.failures|=1;hdcOK=false;}if(isfinite(h)&&h>=0&&h<=100)r.hum=constrain(h+c.humOffset,0.0,100.0);else r.failures|=2;}else r.failures|=3;
 uint8_t b[4];if(reg(0x4a,0,b,4)){uint32_t raw=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);float x=raw*1.4f/1000;
  if(x<=300000){x=max(0.0,(x-c.luxOffset)/c.luxSlope);luxHistory[luxIndex]=x;luxIndex=(luxIndex+1)%15;luxCount=min(luxCount+1,15U);float sorted[15];memcpy(sorted,luxHistory,luxCount*sizeof(float));for(unsigned i=1;i<luxCount;++i){float v=sorted[i];int j=i-1;while(j>=0&&sorted[j]>v){sorted[j+1]=sorted[j];--j;}sorted[j+1]=v;}r.lux=sorted[luxCount/2];}else r.failures|=4;
 }else r.failures|=4;
 uint64_t mv=0;for(unsigned i=0;i<16;++i){mv+=analogReadMilliVolts(SOUND_PIN);delayMicroseconds(200);}r.voltage=mv/(16.0f*1000);r.noise=soundDbA(r.voltage,c.noiseSlope,c.noiseOffset);if(!isfinite(r.noise))r.failures|=16;
 {Lock lock;reading=r;float values[]={r.temp,r.hum,r.lux,r.voltage,r.noise};for(unsigned i=0;i<5;++i)if(isfinite(values[i])){sum[i]+=values[i];++counts[i];}++samples;}
}
// Todas las operaciones I2C y los cambios de CPU se hacen desde el propietario del bus.
// El driver original no informa del resultado de cada transferencia del framebuffer.
bool flushOled(){
 const uint8_t addressing[]={0x20,0x00,0x21,0,127,0x22,0,7};
 for(unsigned attempt=0;attempt<2;++attempt){
  Wire.beginTransmission(oledAddress);Wire.write(uint8_t(0x00));Wire.write(addressing,sizeof addressing);
  bool ok=Wire.endTransmission()==0;
  uint8_t*buffer=oled.getBuffer();
  for(size_t off=0;ok&&off<1024;off+=16){
   Wire.beginTransmission(oledAddress);Wire.write(uint8_t(0x40));
   size_t written=Wire.write(buffer+off,16);int error=Wire.endTransmission();ok=written==16&&error==0;
  }
  if(ok)return true;
  delay(5);
 }
 static uint64_t lastError=0;if(!lastError||nowUs()-lastError>=10000000){lastError=nowUs();Serial.println("[OLED] Error I2C enviando imagen; revisar alimentacion y conexiones.");}
 return false;
}
void label(int y,const char*name,float value,const char*unit,int digits=1){oled.setTextSize(1);oled.setCursor(0,y);oled.print(name);oled.setCursor(58,y);if(isfinite(value))oled.print(value,digits);else oled.print("--");oled.print(unit);}
void draw(const Config&c){
 Reading r;Delivery d[DESTINATIONS];{Lock lock;r=reading;memcpy(d,delivery,sizeof d);}
 oled.clearDisplay();oled.setTextColor(SSD1306_WHITE);oled.setTextSize(1);oled.setCursor(0,0);String id=c.id;if(id.length()>14)id=id.substring(0,14);oled.print(id);
 if(maintenanceActive())oled.print(" M");
 if(sending){if((millis()/200)%2)oled.fillCircle(123,3,2,SSD1306_WHITE);}else oled.drawCircle(123,3,2,SSD1306_WHITE);
 oled.drawLine(0,11,127,11,SSD1306_WHITE);
 switch(page){
 case 0:label(17,"Temp",r.temp," C");label(29,"Humedad",r.hum," %");label(41,"Luz",r.lux," lx",0);label(53,"Sonido",r.noise," dBA",1);break;
 case 1:oled.setCursor(0,16);oled.print("Temperatura / Humedad");oled.setTextSize(2);oled.setCursor(0,30);if(isfinite(r.temp))oled.print(r.temp,1);else oled.print("--");oled.setTextSize(1);oled.print(" C");oled.setTextSize(2);oled.setCursor(0,49);if(isfinite(r.hum))oled.print(r.hum,1);else oled.print("--");oled.setTextSize(1);oled.print(" %");break;
 case 2:oled.setCursor(0,16);oled.print("Iluminacion");oled.setTextSize(2);oled.setCursor(0,32);if(isfinite(r.lux))oled.print(r.lux,0);else oled.print("--");oled.setTextSize(1);oled.setCursor(0,54);oled.print("lux | mediana 15");break;
 case 3:oled.setCursor(0,16);oled.print("SEN0232 | A / Fast");label(32,"Nivel",r.noise," dBA",1);label(44,"ADC",r.voltage*1000," mV",0);oled.setCursor(0,56);oled.print(c.noiseVerified?"Ref. campo; no Leq":"Fabr./ajuste; no Leq");break;
 default:oled.setCursor(0,16);oled.print(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"WiFi sin conexion");oled.setCursor(0,26);oled.printf("RSSI %d | %uK RAM",WiFi.status()==WL_CONNECTED?WiFi.RSSI():0,ESP.getFreeHeap()/1024);oled.setCursor(0,36);oled.printf("R:%lus E:%lus",c.readMs/1000,c.sendMs/1000);oled.setCursor(0,46);uint32_t pending=0;for(auto&x:d)pending+=x.pending;oled.printf("Cola:%u UTC:%s",pending,clockReady()?"OK":"--");oled.setCursor(0,56);oled.print(maintenanceActive()?"http://"+(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():String("192.168.4.1")):String("4s: mantenimiento"));break;
 }
 flushOled();
}
void button(){uint64_t now=nowUs();bool b=digitalRead(BUTTON_PIN)==LOW;if(b!=raw){raw=b;debounce=now;}if(now-debounce<50000)return;
 if(b!=pressed){pressed=b;if(b){pressedAt=now;longHandled=false;recoveryHandled=false;}else if(!longHandled){if(on)page=(page+1)%5;interaction=now;lastFrame=0;}}
 if(pressed&&!longHandled&&now-pressedAt>=4000000){longHandled=true;interaction=now;lastFrame=0;requestMaintenance(600);page=4;}
 if(pressed&&!recoveryHandled&&now-pressedAt>=10000000){recoveryHandled=true;longHandled=true;interaction=now;lastFrame=0;requestRecovery(600);page=4;}
}
bool closeInterval(const Config&c){Reading avg;uint32_t n,valid[5];uint64_t start;
 {Lock lock;avg=reading;n=samples;start=intervalStart;float*v[]={&avg.temp,&avg.hum,&avg.lux,&avg.voltage,&avg.noise};avg.failures=0;for(unsigned i=0;i<5;++i){*v[i]=counts[i]?sum[i]/counts[i]:NAN;if(!counts[i])avg.failures|=1U<<i;}avg.uptimeUs=nowUs();avg.utc=clockReady()?time(nullptr):0;}
 memcpy(valid,counts,sizeof valid);if(!n)return true;
 // El registro es una ventana cerrada de muestras; el HTTPS no determina sus limites.
 String payload=buildPayload(c,avg,++sequence,boot,start,n,valid);bool persisted=c.id==snapshot().id?enqueue(payload):true;
 {Lock lock;memset(sum,0,sizeof sum);memset(counts,0,sizeof counts);samples=0;intervalStart=nowUs();}
 return persisted;
}
}
void beginSensors(){
 sensorCpuMHz=snapshot().cpuMHz;
 if(!setCpuFrequencyMhz(sensorCpuMHz)){sensorCpuMHz=getCpuFrequencyMhz();Serial.println("[CPU] No se pudo aplicar la frecuencia solicitada.");}
 Serial.printf("[CPU] %u MHz antes de iniciar I2C\n",sensorCpuMHz);
 pinMode(BUTTON_PIN,INPUT_PULLUP);pinMode(LUX_ENABLE_PIN,OUTPUT);
 digitalWrite(LUX_ENABLE_PIN,LOW);delay(1000);digitalWrite(LUX_ENABLE_PIN,HIGH);
 analogReadResolution(12);analogSetPinAttenuation(SOUND_PIN,ADC_11db);
 bool busOK=Wire.begin(SDA_PIN,SCL_PIN);Wire.setClock(50000);Wire.setTimeOut(100);
 Serial.printf("[I2C] SDA=%d SCL=%d 50000Hz inicio=%s\n",SDA_PIN,SCL_PIN,busOK?"OK":"ERROR");
 unsigned devices=0;bool screen3c=false,screen3d=false;
 for(uint8_t addr=1;addr<127;++addr){Wire.beginTransmission(addr);if(Wire.endTransmission()==0){
  ++devices;Serial.printf("[I2C] Encontrado 0x%02X\n",addr);if(addr==0x3c)screen3c=true;if(addr==0x3d)screen3d=true;
 }}
 if(!devices)Serial.println("[I2C] Ningun dispositivo: comprobar alimentacion, GND, SDA, SCL y resistencias pull-up.");
 uint8_t screenAddress=screen3c?0x3c:0x3d;oledAddress=screenAddress;
 oledOK=(screen3c||screen3d)&&oled.begin(SSD1306_SWITCHCAPVCC,screenAddress,true,false);
 Serial.printf("[OLED] SSD1306 128x64 direccion=0x%02X resultado=%s\n",screenAddress,oledOK?"INICIALIZADO":"NO DETECTADO / SIN MEMORIA");
 if(oledOK){oled.clearDisplay();oled.setTextColor(SSD1306_WHITE);oled.setTextSize(2);oled.setCursor(0,0);oled.print("TARS");
  oled.setTextSize(1);oled.setCursor(0,25);oled.print("Arranque 4.1.1");oled.setCursor(0,40);oled.print("Inicializando...");flushOled();}
 checkHdc();Serial.printf("[HDC1080] %s\n",hdcOK?"OK":"No identificado");
 Wire.beginTransmission(0x40);Wire.write(2);Wire.write(0);Wire.write(0);Wire.endTransmission();
 boot=randomHex(16);intervalStart=nowUs();interaction=intervalStart;
}
void oledPage(){page=(page+1)%5;interaction=nowUs();lastFrame=0;}
void sensorTask(void*){Serial.println("[SENSORES] Tarea iniciada");uint64_t lastRead=0,lastClose=nowUs();Config previous=snapshot();
 Config c=previous;for(;;){if(c.version!=configVersion())c=snapshot();
  if(sensorCpuMHz!=c.cpuMHz){
   // Nunca cambiar el reloj desde la tarea de red durante una transferencia I2C.
   if(setCpuFrequencyMhz(c.cpuMHz)){sensorCpuMHz=c.cpuMHz;Wire.setClock(50000);Serial.printf("[CPU] %u MHz aplicado entre lecturas\n",sensorCpuMHz);}
  }
  uint64_t now=nowUs();button();
  if(c.version!=previous.version){closeInterval(previous);luxCount=luxIndex=0;lastClose=now;previous=c;}
  if(now-lastRead>=uint64_t(c.readMs)*1000||!lastRead){readSensors(c);lastRead=nowUs();}
  if(boundaryRequested){boundaryDone=closeInterval(c);lastClose=nowUs();boundaryRequested=false;}
  else if(now-lastClose>=uint64_t(c.sendMs)*1000&&!AmeriOta::active()){closeInterval(c);lastClose=nowUs();}
  if(c.pageMs&&now-lastPage>=uint64_t(c.pageMs)*1000){page=(page+1)%5;lastPage=now;}
  uint64_t displayNow=nowUs();bool wanted=!c.displayMs||displayNow<interaction||displayNow-interaction<uint64_t(c.displayMs)*1000;
  if(oledOK){if(brightness!=c.brightness){oled.ssd1306_command(SSD1306_SETCONTRAST);oled.ssd1306_command(c.brightness);brightness=c.brightness;}
   if(wanted!=on){on=wanted;oled.ssd1306_command(on?SSD1306_DISPLAYON:SSD1306_DISPLAYOFF);}
   if(on&&displayNow-lastFrame>=500000){draw(c);lastFrame=displayNow;}
  }vTaskDelay(pdMS_TO_TICKS(20));
 }
}
// Antes de una OTA, las muestras en curso permanecen activas hasta el reinicio.
// Los registros ya cerrados se escribieron de forma duradera al encolarse.
bool persistBoundary(){if(!storageReady)return false;boundaryDone=false;boundaryRequested=true;uint64_t end=nowUs()+3000000;while(!boundaryDone&&nowUs()<end)vTaskDelay(pdMS_TO_TICKS(10));return boundaryDone;}
}
