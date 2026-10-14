#pragma once
#include <Arduino.h>
namespace AmeriOta {
struct Safety {bool busy,valveClosed,selfTest;uint32_t pending;bool otaEnabled=true;
 Safety(bool b=true,bool c=false,bool t=false,uint32_t p=0):busy(b),valveClosed(c),selfTest(t),pending(p){}
};
struct Network {String base,ca;};
bool begin();
void rollbackIfPending();
void tick(const Safety&,const Network&,bool (*boundary)());
bool active();bool ready();String diagnostics();
bool takeMeasurementGap();
}
