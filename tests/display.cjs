const path=require('path');
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const s=fs.readFileSync(path.join(__dirname,'../firmware/TARS/TarsSensors.cpp'),'utf8');
let f=s.slice(s.indexOf('bool flushOled(){'),s.indexOf('\nvoid label('));
f=f.replace('bool flushOled()','function flushOled()').replace('const uint8_t addressing[]={','const addressing=[').replace('0,7};','0,7];').replaceAll('sizeof addressing','addressing.length').replace('unsigned attempt=','let attempt=').replace('size_t off=','let off=').replace('bool ok=','let ok=').replace('uint8_t*buffer=','let buffer=').replace('size_t written=','let written=').replace('int error=','let error=').replace('static uint64_t lastError=0;','').replace('buffer+off,16','buffer.subarray(off,off+16),16');
function run(fail){const a={uint8_t:Number,oledAddress:60,lastError:0,time:1,transactions:[],current:[],data:new Uint8Array(1024),failed:false};for(let i=0;i<1024;i++)a.data[i]=i%256;a.oled={getBuffer:()=>a.data};a.nowUs=()=>a.time;a.delay=()=>{};a.Serial={println(){}};
 a.Wire={beginTransmission(addr){assert.equal(addr,60);a.current=[]},write(v,n){if(typeof v==='number'){a.current.push(v);return 1;}a.current.push(...v);return n},endTransmission(){a.transactions.push(a.current);if(fail&&!a.failed&&a.transactions.length===5){a.failed=true;return 2;}return 0;}};
 vm.createContext(a);vm.runInContext(f,a);assert.equal(a.flushOled(),true);return a;}
let a=run(false);assert.equal(a.transactions.length,65);assert.deepEqual(a.transactions[0],[0,32,0,33,0,127,34,0,7]);const bytes=[];for(let t of a.transactions.slice(1)){assert.equal(t[0],64);assert.equal(t.length,17);bytes.push(...t.slice(1));}assert.deepEqual(bytes,Array.from(a.data));
a=run(true);assert.equal(a.transactions.length,70);assert.deepEqual(a.transactions[5],[0,32,0,33,0,127,34,0,7]);
const network=fs.readFileSync(path.join(__dirname,'../firmware/TARS/TarsNetwork.cpp'),'utf8');assert(!network.includes('setCpuFrequencyMhz('));assert(s.indexOf('setCpuFrequencyMhz(sensorCpuMHz)')<s.indexOf('bool busOK=Wire.begin'));
console.log('PASS: 1024 framebuffer bytes in 64 checked chunks; explicit SSD1306 addressing; NACK retries whole frame; CPU change removed from network task and applied before bus startup.');
