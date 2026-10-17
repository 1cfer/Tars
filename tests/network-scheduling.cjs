const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'../firmware/TARS');
const ino=fs.readFileSync(path.join(root,'TARS.ino'),'utf8');
const ota=fs.readFileSync(path.join(root,'AmeriOta.cpp'),'utf8');
assert(ino.includes('"tars-network",16384,nullptr,tskIDLE_PRIORITY,&network,0'));
assert(ota.includes('"ameri-ota",20480,nullptr,tskIDLE_PRIORITY,&task,0'));
assert(ino.includes('static_assert(configUSE_PREEMPTION && configUSE_TIME_SLICING'));
// Regression model: a CPU-bound TLS segment, no voluntary blocking for 12s.
// With equal priority and preemptive time slicing IDLE runs between TLS slices.
function longestIdleGap(workerPriority){let gap=0,max=0;for(let tick=0;tick<1200;tick++){
 const idleRuns=workerPriority===0 && tick%2===0;
 gap=idleRuns?0:gap+10;max=Math.max(max,gap);
}return max;}
assert(longestIdleGap(1)>5000,'reproduces prior starvation');
assert(longestIdleGap(0)<5000,'idle can run inside CPU-bound TLS');
for(const name of ['TARS.ino','TarsNetwork.cpp','AmeriOta.cpp']){
 const s=fs.readFileSync(path.join(root,name),'utf8');
 assert(!/esp_task_wdt_(deinit|delete|reconfigure)|disableCore0WDT/.test(s));
}
console.log('PASS: network and OTA share IDLE priority; preemption/time slicing required; watchdog not disabled. Scheduler model only, not a hardware TLS test.');
