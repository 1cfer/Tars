const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const root=path.join(__dirname,'../firmware/TARS'),source=fs.readFileSync(path.join(root,'TarsSensors.cpp'),'utf8').replace(/\r\n/g,'\n');
let f=source.slice(source.indexOf('float soundDbA('),source.indexOf('\nvoid readSensors'));
f=f.replace(/float soundDbA\([^)]*\)/,'function soundDbA(voltage,slope,offset)').replace('double value=','let value=').replaceAll('0.6f','0.6').replaceAll('2.6f','2.6').replace('float(value)','Number(value)');
const ctx={isfinite:Number.isFinite,NAN:NaN};vm.createContext(ctx);vm.runInContext(f,ctx);
for(const [v,db]of [[.6,30],[1,50],[1.252,62.6],[1.6,80],[2,100],[2.6,130]])assert(Math.abs(ctx.soundDbA(v,50,0)-db)<1e-4);
for(const v of [0,.59,2.61,3.3,5,NaN,Infinity])assert(Number.isNaN(ctx.soundDbA(v,50,0)));
assert.equal(ctx.soundDbA(1.2,50,1),61);assert(Number.isNaN(ctx.soundDbA(2.6,50,1)));
const payload=fs.readFileSync(path.join(root,'TarsPayload.cpp'),'utf8');assert(payload.includes('num("noise",soundVerified?r.noise:NAN,"2N")'));assert(!payload.includes('c.noiseVerified?r.noise:NAN'));assert(payload.includes('A_weighted_Fast_sample_mean;not_Leq'));assert(payload.includes('sensorTimeConstantMs'));assert(payload.includes('valid["noise"]=soundVerified?counts[4]:0'));
const page=fs.readFileSync(path.join(root,'TarsPage.h'),'utf8');assert(page.includes("['noise_dba','Nivel sonoro','dBA']"));assert(source.includes('analogReadMilliVolts(SOUND_PIN)'));
console.log('PASS: SEN0232 factory transfer 30-130 dBA, examples including 1.252V=62.6dBA, invalid voltage/output -> NaN, retained adjustment, dBA UI and payload weighting/mean metadata. No hardware calibration test.');

const basis=payload.match(/bool soundVerified=([^;]+);/)[1];for(const [c,want]of [[{noiseSlope:50,noiseOffset:0,noiseVerified:false},true],[{noiseSlope:49,noiseOffset:0,noiseVerified:false},false],[{noiseSlope:49,noiseOffset:0,noiseVerified:true},true]])assert.equal(vm.runInNewContext(basis,{c}),want);
