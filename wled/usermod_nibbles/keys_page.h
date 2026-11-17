// The brains' /keys page: which key each keypad switch is, changed from a
// phone on the Nibbles network. The page asks the base through this brain
// (/keys/ask, /keys/answer; NL_MSG_CONSOLE) with the base's own `keymap`
// serial commands, and draws the boards from the answers. Reached from
// WLED's Info panel ("Nibbles pages"), since a captive-portal sheet can't
// type an address.
#pragma once

static const char NIBBLES_KEYS_PAGE[] PROGMEM = R"html(<!doctype html>
<html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>Nibbles keys</title><style>
:root{color-scheme:dark}
body{margin:0;padding:12px 16px 40px;background:#111;color:#eee;font:16px/1.35 system-ui,sans-serif}
a{color:#8cf}h1{font-size:20px;margin:4px 0 8px}
#st{min-height:1.4em;color:#aaa;font-size:14px}#st.bad{color:#f88}
.card{background:#1c1c1c;border-radius:10px;padding:10px;margin:12px 0}
.card h2{font-size:16px;margin:0 0 8px;display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.card h2 small{color:#999;font-weight:normal}.card h2 button{margin-left:auto}
.row{display:grid;grid-template-columns:repeat(4,1fr);gap:6px}
.k{border:2px solid #333;border-radius:8px;padding:6px 2px;min-height:54px;text-align:center;color:#fff;background:#222;font:inherit}
.k b{display:block;font-size:15px;word-break:break-all}.k i{font-style:normal;font-size:11px;opacity:.75}
.k.dark{color:#000}.k.hit{border-color:#fff;box-shadow:0 0 0 3px #ff0}
button{font:inherit;background:#333;color:#eee;border:0;border-radius:6px;padding:6px 12px}
button.go{background:#2a6;color:#fff}
#sheet{position:fixed;inset:0;background:#000a;display:none;align-items:flex-end;justify-content:center}
#sheet>div{background:#222;width:100%;max-width:480px;border-radius:12px 12px 0 0;padding:16px}
select{font:inherit;width:100%;padding:8px;margin:10px 0;background:#111;color:#eee;border:1px solid #444;border-radius:6px}
#out{white-space:pre-wrap;font:12px monospace;color:#aaa}
</style></head><body>
<a href="/">&larr; WLED</a>
<h1>Nibbles keys</h1>
<div id=st>Asking the base...</div>
<div id=last></div>
<div id=boards></div>
<p><button id=resetall>Reset every board</button></p>
<p style="color:#999;font-size:14px">Press a key on the keypad and it is outlined here, to find which switch is which. Tap a switch to give it another key. Changes are saved on the base at once.</p>
<div id=out></div>
<div id=sheet><div><div id=sh></div><select id=pick></select>
<button class=go id=save>Save</button> <button id=cancel>Cancel</button></div></div>
<script>
const $=id=>document.getElementById(id),sleep=ms=>new Promise(r=>setTimeout(r,ms));
let keys=[],boards={},last=null,queue=Promise.resolve(),edit=null,fails=0;
function ask(c){const p=queue.then(()=>ask1(c));queue=p.catch(()=>{});return p}
async function ask1(c){
 for(let t=0;t<3;t++){
  const r=await fetch('/keys/ask?c='+encodeURIComponent(c));
  if(!r.ok)throw new Error(await r.text());
  const id=(await r.json()).id;
  for(let i=0;i<15;i++){await sleep(80);const a=await fetch('/keys/answer?id='+id);
   if(a.status==200)return await a.text();if(a.status!=202)break}
 }
 throw new Error('No answer from the base')
}
function status(t,bad){$('st').textContent=t;$('st').className=bad?'bad':''}
function parse(text){
 for(const l of text.split('\n')){
  let m;
  if(m=l.match(/^KEYMAP (\d+) (0x[0-9a-f]+)( \(prototype\))? (\S+)( \(changed\))?$/)){
   const b=boards[m[1]]||(boards[m[1]]={});b.addr=m[2];b.proto=!!m[3];b.keys=m[4].split(',');b.changed=!!m[5]}
  else if(m=l.match(/^KEYCOLORS (\d+) (\S*)$/)){if(boards[m[1]])boards[m[1]].colors=m[2].split(',')}
  else if(m=l.match(/^KEYFOUND (\S+)$/)){m[1].split(',').forEach((f,i)=>{if(boards[i+1])boards[i+1].found=f=='1'})}
  else if(m=l.match(/^KEYLAST (\d+) (\d+) (\S+) (\d+)$/))last={b:+m[1],s:+m[2],name:m[3],ms:+m[4]};
  else if(l=='KEYLAST none')last=null;
  else if(l.startsWith('KEYMAP')||l.startsWith('ERROR'))$('out').textContent=l;
 }
}
function render(){
 const recent=last&&last.ms<8000;
 $('last').textContent=last?'Last pressed: board '+last.b+', switch '+last.s+' ('+last.name+'), '+Math.round(last.ms/1000)+' s ago':'';
 let h='';
 for(const n of Object.keys(boards)){const b=boards[n];if(!b.keys)continue;
  h+='<div class=card><h2>Board '+n+' <small>'+b.addr+(b.proto?' prototype':'')+(b.found===false?' &middot; not connected':'')+(b.changed?' &middot; changed':'')+'</small><button data-reset='+n+'>Reset</button></h2><div class=row>';
  b.keys.forEach((k,i)=>{const c=(b.colors&&b.colors[i])||'000000',v=parseInt(c,16),
   lum=((v>>16)*299+((v>>8)&255)*587+(v&255)*114)/1000;
   h+='<button class="k'+(lum>140?' dark':'')+(recent&&last.b==n&&last.s==i+1?' hit':'')+'" style="background:#'+(v?c:'222')+'" data-b='+n+' data-s='+(i+1)+'><i>switch '+(i+1)+'</i><b>'+k+'</b></button>'});
  h+='</div></div>'}
 $('boards').innerHTML=h;
}
async function refresh(){
 try{parse(await ask('keymap all'));render();fails=0;status('Base answering')}
 catch(e){if(++fails>1)status(e.message,1)}
}
async function run(c){
 try{const t=await ask(c);$('out').textContent=t.trim();parse(t);await refresh()}catch(e){status(e.message,1)}
}
$('boards').onclick=e=>{
 const r=e.target.closest('[data-reset]');
 if(r){if(confirm('Put board '+r.dataset.reset+' back to its built-in keys?'))run('keymap '+r.dataset.reset+' reset');return}
 const k=e.target.closest('.k');if(!k)return;
 edit={b:k.dataset.b,s:k.dataset.s};const now=boards[edit.b].keys[edit.s-1];
 $('sh').textContent='Board '+edit.b+', switch '+edit.s+': now '+now;
 $('pick').innerHTML=['none',...keys].map(n=>'<option'+(n==now?' selected':'')+'>'+n+'</option>').join('');
 $('sheet').style.display='flex'};
$('cancel').onclick=()=>{$('sheet').style.display='none';edit=null};
$('save').onclick=()=>{const e=edit;$('sheet').style.display='none';edit=null;if(e)run('keymap '+e.b+' '+e.s+' '+$('pick').value)};
$('resetall').onclick=()=>{if(confirm('Put every board back to its built-in keys?'))run('keymap reset')};
(async()=>{
 for(;;){try{keys=(await ask('keys')).trim().split(/\s+/);break}catch(e){status(e.message,1);await sleep(2000)}}
 for(;;){if(!edit&&!document.hidden)await refresh();await sleep(1200)}
})();
</script></body></html>)html";
