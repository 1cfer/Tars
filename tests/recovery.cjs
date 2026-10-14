const path=require('path');
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const s=fs.readFileSync(path.join(__dirname,'../firmware/TARS/TarsSensors.cpp'),'utf8');
let b=s.slice(s.indexOf('void button(){'),s.indexOf('\nbool closeInterval')).replace('void button()','function button()').replace('uint64_t now=','let now=').replace('bool b=','let b=');
let d=s.slice(s.indexOf('uint64_t displayNow='),s.indexOf('}vTaskDelay(pdMS_TO_TICKS(20))')).replace('uint64_t displayNow=','let displayNow=').replace('bool wanted=','let wanted=');
const ctx={uint64_t:Number,time:0,low:false,LOW:0,nowUs(){return this.time},digitalRead(){return ctx.low?0:1},BUTTON_PIN:27,on:false,page:2,interaction:0,lastFrame:0,pressed:false,raw:false,longHandled:false,recoveryHandled:false,debounce:0,pressedAt:0,oledOK:true,brightness:32,c:{displayMs:60000,brightness:32},commands:[],draws:0,requests:[],SSD1306_DISPLAYON:175,SSD1306_DISPLAYOFF:174,SSD1306_SETCONTRAST:129,oled:{ssd1306_command(n){ctx.commands.push(n)}},draw(){ctx.draws++},requestMaintenance(n){ctx.requests.push(n)},recoveryRequests:[],requestRecovery(n){ctx.recoveryRequests.push(n)}};
// Bind the simulated clock explicitly; the function bodies below are extracted from delivered C++.
ctx.nowUs=()=>ctx.time;vm.createContext(ctx);vm.runInContext(b+'\nfunction frame(){'+d+'}}',ctx);
function step(t,low){ctx.time=t;ctx.low=low;ctx.button()}
step(100000,true);step(160000,true);step(200000,false);step(260000,false);
assert.equal(ctx.on,false,'button must not mark physical screen ON prematurely');assert.equal(ctx.page,2,'wake keeps current page');
ctx.frame();assert.equal(ctx.on,true);assert(ctx.commands.includes(175),'wake must send SSD1306 DISPLAYON');
step(300000,true);step(360000,true);step(400000,false);step(460000,false);assert.equal(ctx.page,3,'next click advances page');
ctx.time=61000000;ctx.frame();assert.equal(ctx.on,false);assert.equal(ctx.commands.at(-1),174);
step(62000000,true);step(62060000,true);step(66100000,true);assert.deepEqual(ctx.requests,[600]);ctx.frame();assert.equal(ctx.on,true);assert.equal(ctx.commands.at(-1),175);
const ino=fs.readFileSync(path.join(__dirname,'../firmware/TARS/TARS.ino'),'utf8');const setup=ino.slice(ino.indexOf('void setup()'),ino.indexOf('void loop()'));
assert(!setup.includes('requestMaintenance('),'startup must not open maintenance');
console.log('PASS: wake sends physical ON; next click changes page; inactivity sends OFF; long press opens maintenance and wakes; no startup maintenance.');



step(72100000,true);assert.deepEqual(ctx.recoveryRequests,[600]);
step(73100000,true);step(74100000,true);assert.deepEqual(ctx.recoveryRequests,[600],'held button must not renew recovery');
step(74200000,false);step(74260000,false);assert.equal(ctx.page,4,'release after long hold must not change page');

const m=fs.readFileSync(path.join(__dirname,'../firmware/TARS/TarsMaintenance.cpp'),'utf8');
let loop=m.slice(m.indexOf('void maintenanceLoop(){'),m.lastIndexOf('\n}')).replace('void maintenanceLoop()','function maintenanceLoop()').replace('uint32_t seconds=','let seconds=').replace('uint64_t end=','let end=').replace('static uint64_t nextAp=0;','').replace('bool force=','let force=').replaceAll('0x80000000U','0x80000000').replaceAll('0x7fffffffU','0x7fffffff');
const a={requestedMaintenance:0,UINT32_MAX:4294967295,time:0,until:0,nextAp:0,connected:false,WL_CONNECTED:3,maintenanceAp:false,forcedAp:false,localUpload:false,rebootAt:0,opens:0,closes:0};
a.nowUs=()=>a.time;a.deadline=()=>a.until;a.maintenanceActive=()=>a.until>a.time;a.WiFi={status:()=>a.connected?3:0};a.stopAp=()=>a.maintenanceAp=false;a.startAp=()=>a.maintenanceAp=true;a.close=()=>{a.closes++;a.until=0;a.maintenanceAp=false;a.forcedAp=false};a.open=(n,force=false)=>{a.opens++;a.until=a.time+n*1e6;a.forcedAp=force;a.maintenanceAp=force||!a.connected};a.web={handleClient(){}};a.ESP={restart(){throw Error('unexpected restart')}};
vm.createContext(a);vm.runInContext(loop,a);
a.maintenanceLoop();assert(!a.maintenanceAp);assert.equal(a.opens,0);
a.connected=true;a.requestedMaintenance=600;a.maintenanceLoop();assert(!a.maintenanceAp);const end=a.until;
a.time=10000000;a.connected=false;a.maintenanceLoop();assert(a.maintenanceAp);assert.equal(a.until,end);
a.connected=true;a.maintenanceLoop();assert(!a.maintenanceAp);assert.equal(a.until,end);
a.connected=false;a.localUpload=true;a.maintenanceLoop();assert(!a.maintenanceAp,'no interface change during upload');a.localUpload=false;
a.time=end;a.maintenanceLoop();assert.equal(a.until,0);assert(!a.maintenanceAp);assert.equal(a.closes,1);
console.log('PASS: no spontaneous AP; connected maintenance uses STA; disconnect fallback AP; reconnect removes AP; switching preserves deadline; upload prevents interface switching; timeout closes.');

a.time=end+1;a.connected=true;a.requestedMaintenance=(600|0x80000000)>>>0;a.maintenanceLoop();assert(a.forcedAp&&a.maintenanceAp,'forced recovery must open AP with STA connected');
const forcedEnd=a.until;a.time+=10000000;a.maintenanceLoop();assert(a.maintenanceAp);assert.equal(a.until,forcedEnd);
a.connected=false;a.time+=10000000;a.maintenanceLoop();assert(a.maintenanceAp);a.connected=true;a.maintenanceLoop();assert(a.maintenanceAp,'reconnect must not remove forced AP');
a.time=forcedEnd;a.maintenanceLoop();assert(!a.maintenanceAp&&!a.forcedAp);assert.equal(a.until,0);
a.requestedMaintenance=(600|0x80000000)>>>0;a.maintenanceLoop();assert(a.maintenanceAp);a.requestedMaintenance=a.UINT32_MAX;a.maintenanceLoop();assert(!a.maintenanceAp&&!a.forcedAp);
console.log('PASS: physical 10 s gesture fires once; release does not navigate; forced AP survives connection/reconnection; expiration and explicit close remove it.');
