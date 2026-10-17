const path=require('path'),fs=require('fs'),vm=require('vm'),assert=require('assert');
const s=fs.readFileSync(path.join(__dirname,'../firmware/TARS/TarsSensors.cpp'),'utf8');
let f=s.slice(s.indexOf('bool flushOled(){'),s.indexOf('\nvoid label('));
f=f.replace('bool flushOled()','function flushOled()').replace('uint8_t*buffer=','let buffer=')
 .replace('const uint8_t addressing[]={','const addressing=[').replace('uint8_t(row)};','uint8_t(row)];')
 .replaceAll('sizeof addressing','addressing.length').replaceAll('sizeof shown','shown.length')
 .replace(/\b(unsigned|size_t|bool|int) (attempt|row|base|off|length|ok|written|error)=/g,'let $2=')
 .replace('static uint64_t lastError=0;','').replace('uint64_t errorNow=','let errorNow=')
 .replace('buffer+base+off,length','buffer.subarray(base+off,base+off+length),length');
const a={size_t:Number,uint8_t:Number,min:Math.min,oledAddress:60,lastError:0,time:1,transactions:[],current:[],data:new Uint8Array(1024),shown:new Uint8Array(1024),shownValid:false,screen:new Uint8Array(1024),failAt:0,cursor:0,end:127,row:0};
for(let i=0;i<1024;i++)a.data[i]=i%256;
a.oled={getBuffer:()=>a.data};a.nowUs=()=>a.time;a.delay=()=>{};a.Serial={println(){}};a.memcpy=(out,input)=>out.set(input);
a.Wire={beginTransmission(addr){assert.equal(addr,60);a.current=[]},write(v,n){if(typeof v==='number'){a.current.push(v);return 1;}a.current.push(...v);return n},endTransmission(){
 a.transactions.push(a.current);if(a.failAt===a.transactions.length)return 2;
 if(a.current[0]===0){assert.equal(a.current.length,9);a.cursor=a.current[4];a.end=a.current[5];a.row=a.current[7];assert.equal(a.current[8],a.row);}
 else {assert.equal(a.current[0],64);for(const b of a.current.slice(1)){a.screen[a.row*128+a.cursor]=b;if(++a.cursor>a.end)a.cursor=a.current[4];}}
 return 0;
}};
vm.createContext(a);vm.runInContext(f,a);
assert(a.flushOled());assert.equal(a.transactions.length,72);assert.deepEqual(a.screen,a.data);
a.transactions=[];assert(a.flushOled());assert.equal(a.transactions.length,0,'unchanged frame sends no I2C');
a.data[300]^=255;assert(a.flushOled());assert.equal(a.transactions.length,2,'one dirty byte needs one address command and one data transfer');assert.deepEqual(a.screen,a.data);
a.transactions=[];a.data[400]^=255;a.failAt=2;assert(a.flushOled());assert.equal(a.transactions.length,74,'NACK invalidates cache and retries all eight pages');assert.deepEqual(a.screen,a.data);
a.transactions=[];a.failAt=0;assert(a.flushOled());assert.equal(a.transactions.length,0);
const network=fs.readFileSync(path.join(__dirname,'../firmware/TARS/TarsNetwork.cpp'),'utf8');assert(!network.includes('setCpuFrequencyMhz('));assert(s.indexOf('setCpuFrequencyMhz(sensorCpuMHz)')<s.indexOf('bool busOK=Wire.begin'));
console.log('PASS: dirty-page transfers, zero traffic for identical frames, one-byte updates, NACK full recovery, bus remains 50kHz. Mock I2C only.');
