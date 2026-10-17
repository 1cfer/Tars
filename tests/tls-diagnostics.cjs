const fs=require('fs'),path=require('path'),assert=require('assert'),zlib=require('zlib'),crypto=require('crypto'),vm=require('vm');
const root=path.join(__dirname,'../firmware/TARS');
const n=fs.readFileSync(path.join(root,'TarsNetwork.cpp'),'utf8');
assert(n.includes('WiFiClientSecure tls;tls.setCACert(ca.c_str());tls.setHandshakeTimeout(8);HTTPClient http;'));
assert(n.includes('http.begin(tls,url)'));
assert(n.includes('http.setConnectTimeout(5000);http.setTimeout(5000)'));
assert(!n.includes('setInsecure('));assert(!n.includes('probeRequested'));
// Gzipping the local SHA helper must preserve exact source and hash behavior.
const sha=fs.readFileSync(path.join(root,'Sha256Page.h'),'utf8').replace(/\r\n/g,'\n').split('R"JS(')[1].split(')JS";')[0];
const bytes=fs.readFileSync(path.join(root,'Sha256PageGzip.h'),'utf8').match(/0x[0-9a-f]{2}/g).map(x=>parseInt(x,16));assert.equal(zlib.gunzipSync(Buffer.from(bytes)).toString('utf8'),sha);
const ctx={};vm.createContext(ctx);vm.runInContext(sha,ctx);for(const text of ['', 'abc']){const arr=new TextEncoder().encode(text);assert.equal(ctx.sha256Hex(arr.buffer),crypto.createHash('sha256').update(arr).digest('hex'))}

console.log('PASS: normal TLS transport retains active CA, hostname URL and timeouts; no insecure fallback; completed probe retired; compressed SHA helper unchanged.');
