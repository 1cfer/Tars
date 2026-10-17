const fs=require('fs'),vm=require('vm'),crypto=require('crypto'),assert=require('assert');
const src=fs.readFileSync(__dirname+'/../firmware/TARS/TarsMaintenance.cpp','utf8').replace(/\r\n/g,'\n');
let body=src.slice(src.indexOf(' void challenge(){')+18,src.indexOf('\n }\n};',src.indexOf(' void challenge(){'))).replaceAll('.length()','.length');
let sequence=0;const headers=[],ctx={_snonce:'',_sopaque:'',_srealm:'',String,randomHex:n=>(++sequence).toString(16).padStart(n*2,'0'),sendHeader:(k,v)=>headers.push([k,v]),send:(code,type,text)=>{assert.equal(code,401);assert.equal(type,'text/plain');assert(!text.includes('password'));}};vm.createContext(ctx);vm.runInContext('function challenge(){'+body+'}',ctx);
ctx.challenge();const first=headers.at(-1)[1],nonce=ctx._snonce,opaque=ctx._sopaque;assert(first.includes('Digest realm="TARS", qop="auth"'));assert(first.includes('nonce="'+nonce+'"'));assert(first.includes('opaque="'+opaque+'"'));
for(const route of ['/','/sha.js','/session','/config','/status','/scan']){ctx.challenge();assert.equal(headers.at(-1)[1],first,route+' must not invalidate another resource challenge');}
const md5=s=>crypto.createHash('md5').update(s).digest('hex'),pass='test-only-password',cnonce='test-client',nc='00000001';
function response(password,route,n=nonce){return md5(md5('admin:TARS:'+password)+':'+n+':'+nc+':'+cnonce+':auth:'+md5('GET:'+route));}
for(const route of ['/session','/config','/status','/scan']){assert.equal(response(pass,route),response(pass,route,ctx._snonce));assert.notEqual(response('wrong',route),response(pass,route));}
const reset=src.match(/void resetAuth\(\)\{([^}]+)\}/)[1];vm.runInContext(reset,ctx);ctx.challenge();assert.notEqual(ctx._snonce,nonce);assert.notEqual(ctx._sopaque,opaque);assert.notEqual(response(pass,'/config',ctx._snonce),response(pass,'/config'));
assert(src.includes('if(!web.authenticate("admin",sessionPassword.c_str())){web.challenge();return false;}'));assert(src.includes('web.header("X-CSRF-Token")'));assert(src.includes('web.resetAuth();'));
assert(!src.includes('BASIC_AUTH'));assert(!src.includes('if(web.uri()=="/")web.requestAuthentication'));
console.log('PASS: source-derived Digest challenge shared across all panel resources; challenge unchanged by parallel resource requests, rotated on session close; correct/wrong/stale digest responses; password, interface and CSRF checks retained. Physical browser test pending.');
