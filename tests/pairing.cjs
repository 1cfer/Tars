// Source-derived protocol guards with mocked Arduino types. No ESP32/hardware execution.
const fs=require('fs'),path=require('path'),vm=require('vm'),crypto=require('crypto'),assert=require('assert');
const root=path.join(__dirname,'../firmware/TARS');
const source=fs.readFileSync(path.join(root,'TarsPairing.cpp'),'utf8').replace(/\r\n/g,'\n');
const sha=s=>crypto.createHash('sha256').update(s,'utf8').digest('hex');
const expression=source.match(/String canonical=([^;]+);/)[1];
const canonical=(id,nonce,payload)=>vm.runInNewContext(expression,{id,nonce,payload,sha});
const sign=(code,id,nonce,payload)=>crypto.createHmac('sha256',Buffer.from(code,'ascii')).update(canonical(id,nonce,payload),'utf8').digest('hex');
const vectors=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/pairing-public.json'),'utf8'));
for(const v of vectors){assert.equal(canonical(v.device_id,v.nonce,v.payload_json),v.canonical_utf8);assert.equal(sign(v.code_ascii,v.device_id,v.nonce,v.payload_json),v.signature);assert.notEqual(crypto.createHmac('sha256',Buffer.from(v.code_ascii,'hex')).update(v.canonical_utf8).digest('hex'),v.signature);assert.notEqual(sign(v.code_ascii,v.device_id,v.nonce,v.payload_json+'\n'),v.signature);}
function strictObject(raw){const stack=[];for(let i=0;i<raw.length;i++){let ch=raw[i];if(ch==='{')stack.push(new Set());else if(ch==='}')stack.pop();else if(ch==='"'){let start=i++;for(;i<raw.length;i++){if(raw[i]==='\\'){i++;continue;}if(raw[i]==='"')break;}let next=i+1;while(/\s/.test(raw[next]||'x'))next++;if(raw[next]===':'){let key=JSON.parse(raw.slice(start,i+1));let set=stack.at(-1);if(!set||set.has(key))throw Error('duplicate');set.add(key);}}}const o=JSON.parse(raw);if(!o||Array.isArray(o)||typeof o!=='object')throw Error('object required');return o;}
let verify=source.slice(source.indexOf('bool verifyResponse('),source.indexOf('\n}\n}\nvoid beginPairing')+2);
verify=verify.replace(/bool verifyResponse\([^)]*\)/,'function verifyResponse(raw,claim,payload)')
 .replace('JsonDocument envelope,p;','let envelope={},p={};').replace('JsonDocument req;','let req={};')
 .replace(/([A-Za-z_]\w*(?:\[(?:"[^"]+"|[A-Za-z_]\w*)\])+).is<String>\(\)/g,'isString($1)')
 .replace(/([A-Za-z_]\w*(?:\[(?:"[^"]+"|[A-Za-z_]\w*)\])+).is<unsigned>\(\)/g,'isUnsigned($1)')
 .replace(/([A-Za-z_]\w*(?:\[(?:"[^"]+"|[A-Za-z_]\w*)\])+).is<bool>\(\)/g,'isBool($1)')
 .replace(/\.as<(?:String|unsigned|bool)>\(\)/g,'').replaceAll('.length()','.length').replace('envelope.size()','Object.keys(envelope).length')
 .replace(/const char\*(strings|allowed)\[\]=\{([^}]+)\};/g,'const $1=[$2];').replace('for(const char*k:strings)','for(const k of strings)').replace('for(const char*k:allowed)','for(const k of allowed)')
 .replace('for(JsonPairConst q:p.as<JsonObjectConst>())','for(const q of Object.entries(p))').replace('q.key().c_str()','q[0]').replaceAll('bool ok=','let ok=').replace(/String (id|control)=/g,'let $1=');
const ctx={sha,sign,lowerHex:(s,n)=>typeof s==='string'&&s.length===n&&/^[0-9a-f]+$/.test(s),equal:(a,b)=>a===b,isString:x=>typeof x==='string',isUnsigned:x=>Number.isInteger(x)&&x>=0,isBool:x=>typeof x==='boolean',WiFi:{macAddress:()=> 'AA:BB:CC:DD:EE:FF'},AMERI_FAMILY:'tars',AMERI_BOARD:'upb-tars-esp32-v1',AMERI_PROJECT:'TARS',AMERI_CHIP_ID:0,validHeader:s=>typeof s==='string'&&s.length<=2048&&!/[\x00-\x1f\x7f]/.test(s),validUrl:(s,base)=>{try{const u=new URL(s);return u.protocol==='https:'&&!u.username&&!u.password&&!u.hash&&(!base||(u.pathname==='/'&&!s.endsWith('/')&&!u.search));}catch{return false}},ameriParseObject:(doc,s)=>{try{Object.assign(doc,strictObject(s));return true}catch{return false}}};
vm.createContext(ctx);vm.runInContext(verify,ctx);
const code='0123456789abcdef0123456789abcdef',id='tars_test_01',nonce='a'.repeat(32),base='https://server.example.org:10000';
const claim={base,request_json:JSON.stringify({device_id:id,nonce,pairing_code:code,physical_id:'AA:BB:CC:DD:EE:FF'})};
const p={device_id:id,physical_id:'AA:BB:CC:DD:EE:FF',family:'tars',board:'upb-tars-esp32-v1',project:'TARS',chip_id:0,control_url:base,ota_base_url:base,control_key:'1'.repeat(64),ap_password:'individual-test-password',history_url:'https://server.example.org/rain/records',history_bearer:'FAKE-TEST-ONLY',preserve_local_configuration:true};
function wrap(payload){const raw=typeof payload==='string'?payload:JSON.stringify(payload);return JSON.stringify({protocol:1,device_id:id,nonce,payload_json:raw,signature:sign(code,id,nonce,raw)});}
assert(ctx.verifyResponse(wrap(p),claim,''));assert(ctx.verifyResponse(wrap(JSON.stringify(p,null,2)),claim,''));
for(const patch of [{device_id:'another'},{physical_id:'11:22:33:44:55:66'},{family:'other'},{board:'other'},{project:'other'},{chip_id:1},{preserve_local_configuration:false},{control_key:'X'.repeat(64)},{ap_password:'short'},{control_url:'http://bad'},{ota_base_url:'http://bad'},{extra_field:true},{history_bearer:''}])assert(!ctx.verifyResponse(wrap({...p,...patch}),claim,''),JSON.stringify(Object.keys(patch)));
const dupe=JSON.stringify(p).replace('"device_id":','"device_id":"wrong","device_id":');assert(!ctx.verifyResponse(wrap(dupe),claim,''));
let e=JSON.parse(wrap(p));e.nonce='b'.repeat(32);assert(!ctx.verifyResponse(JSON.stringify(e),claim,''));e=JSON.parse(wrap(p));e.payload_json+=' ';assert(!ctx.verifyResponse(JSON.stringify(e),claim,''));assert(!ctx.verifyResponse(wrap(p).slice(0,-1),claim,''));
assert(source.includes('record["request_json"]=raw'));assert(source.indexOf('saveJournal(record)')<source.indexOf('void pairingTick'));
assert(!fs.readFileSync(path.join(root,'TarsConfig.cpp'),'utf8').includes('prefs.clear('));
assert(!source.slice(source.indexOf('String pairingStatus()')).includes('pairing_code'));
assert(!source.slice(source.indexOf('String pairingStatus()')).includes('history_bearer'));
const page=fs.readFileSync(path.join(root,'TarsPage.h'),'utf8');for(const script of page.matchAll(/<script>([\s\S]*?)<\/script>/g))new vm.Script(script[1]);
console.log('PASS: server public vectors, exact JSON bytes/ASCII key, source-derived signed-envelope guards, identity/MAC/profile/nonce/type guards, malformed/duplicate JSON, private status and panel JavaScript syntax. No hardware tests.');
const expiryLine=source.split('\n').find(x=>x.includes('fail("expired_request_generate_new_code")'));
const expiryExpr=expiryLine.slice(expiryLine.indexOf('if(')+3,expiryLine.indexOf('){fail')).replaceAll('.as<int64_t>()','').replaceAll('time(nullptr)','clock').replaceAll('600000000ULL','600000000');
const expired=(clock,ready,uptime,first)=>vm.runInNewContext(expiryExpr,{clock,clockReady:()=>ready,record:{created_at:1000,first_attempt_at:first},nowUs:()=>uptime,bootStarted:0});
assert(!expired(1599,true,1,0));assert(expired(1600,true,1,0));assert(!expired(1179,true,1,1000));assert(expired(1180,true,1,1000));assert(expired(0,false,600000000,0));
let committed=source.slice(source.indexOf('bool isCommitted('),source.indexOf('\nbool verifyResponse')).replace(/bool isCommitted\([^)]*\)/,'function isCommitted(d,c)').replaceAll('.length()','.length').replace('d["commit_anchor"].is<String>()','isString(d["commit_anchor"])').replaceAll('.as<String>()','');
const anchorExpr=source.match(/String anchor\([^)]*\)\{return ([^;]+);/)[1];
const anchor=c=>vm.runInNewContext(anchorExpr,{c,sha,WiFi:ctx.WiFi});
const commitCtx={isString:ctx.isString,equal:ctx.equal,anchor};vm.createContext(commitCtx);vm.runInContext(committed,commitCtx);
const before={id:'new',controlUrl:'',controlKey:''},after={id,controlUrl:base,controlKey:'1'.repeat(64)},prepared={commit_anchor:anchor(after)};
assert(!commitCtx.isCommitted(prepared,before),'power loss before commit retains request');assert(commitCtx.isCommitted(prepared,after),'power loss after commit recognizes saved credentials');assert(!commitCtx.isCommitted(prepared,{...after,controlKey:'2'.repeat(64)}));
console.log('PASS: source-derived retry TTL and commit-anchor recognition before/after simulated commit. Persistence hardware not tested.');
const zlib=require('zlib');const pageSource=fs.readFileSync(path.join(root,'TarsPage.h'),'utf8').replace(/\r\n/g,'\n');const html=pageSource.split('R"HTML(')[1].split(')HTML";')[0];const bytes=fs.readFileSync(path.join(root,'TarsPageGzip.h'),'utf8').match(/0x[0-9a-f]{2}/g).map(x=>parseInt(x,16));assert.equal(zlib.gunzipSync(Buffer.from(bytes)).toString('utf8'),html);assert(!html.includes('__CSRF__'));assert(!html.includes('__SHA_JS__'));assert(html.includes('fetch(\'/session\')'));assert(html.includes('src="/sha.js"'));
console.log('PASS: compressed panel exactly matches UTF-8 source and loads session/SHA resources.');
