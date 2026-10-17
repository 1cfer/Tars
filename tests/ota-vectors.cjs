const fs=require('fs'),crypto=require('crypto'),vm=require('vm'),assert=require('assert');
const base=__dirname+'/../firmware/TARS/',h=fs.readFileSync(base+'AmeriOtaVectors.h','utf8'),s=fs.readFileSync(base+'AmeriOta.cpp','utf8');
const value=name=>JSON.parse(h.match(new RegExp(name+'=("[^"\\n]*")'))[1]);
const key=value('AMERI_TEST_CONTROL'),derive=crypto.createHmac('sha256',Buffer.from(key,'ascii')).update('AMERI-OTA-1').digest();assert.equal(derive.toString('hex'),value('AMERI_TEST_DERIVED'));
const sha=x=>crypto.createHash('sha256').update(x,'utf8').digest('hex');
let expr=s.match(/String canonical\([^\n]+?\{return ([^;]+);/)[1].replace('String((unsigned long long)c)','String(c)').replace('String((long long)stamp)','String(stamp)');
let count=0;for(const line of h.split(/\r?\n/).filter(x=>x.startsWith('{"'))){const [kind,body,c,stamp,nonce,canonical,signature]=JSON.parse('['+line.slice(1,-2)+']');const computed=vm.runInNewContext(expr,{kind,id:'celda_1',c,stamp,nonce,body,sha});assert.equal(computed,canonical);assert.equal(crypto.createHmac('sha256',derive).update(computed,'utf8').digest('hex'),signature);JSON.parse(body);count++;}assert.equal(count,3);
assert(s.includes('if(!vectorTest())'));assert(s.includes('AMERI_TEST_DERIVED'));
console.log('PASS: all three OTA public vectors (poll/ack/UTF-8 response), exact canonical bytes and HMAC derivation; firmware retains key-derivation self-test. Host crypto test, not a physical boot test.');
