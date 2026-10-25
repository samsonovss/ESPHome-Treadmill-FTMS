'use strict';
// Diagnostics journal (09.10): what went wrong and when — kept in this browser only (localStorage ring buffer).
// Browser side: SSE silences and reconnects, failed/slow requests, script errors, long frames.
// Board side (from existing entities): reboots (Uptime went back), Reset Reason, min free heap, loop time, Wi-Fi.
const TMDiag=(()=>{
 const KEY='tm.diag.v1',MAX=200,GAP_MS=12000,SLOW_MS=3000,LONG_MS=200;
 let S;try{S=JSON.parse(localStorage.getItem(KEY))||{};}catch{S={};}
 S.ev=Array.isArray(S.ev)?S.ev:[];S.b=S.b||{};
 let saveT=0;const save=()=>{clearTimeout(saveT);saveT=setTimeout(()=>{try{localStorage.setItem(KEY,JSON.stringify(S));}catch{}},500);};
 const listeners=new Set(),emit=()=>{for(const f of listeners)try{f();}catch{}};
 function log(kind,text,extra){const e={t:Date.now(),k:kind,m:String(text).slice(0,240),...extra};const last=S.ev.at(-1);
  if(last&&last.k===kind&&last.m===e.m&&e.t-last.t<60000){last.n=(last.n||1)+1;last.t=e.t;}else S.ev.push(e);
  if(S.ev.length>MAX)S.ev.splice(0,S.ev.length-MAX);save();emit();}
 // ---- requests ----
 const F=window.fetch?.bind(window);
 if(F)window.fetch=async(input,init)=>{const url=String(input?.url??input),p=url.replace(location.origin,'').slice(0,80),t0=performance.now();
  try{const r=await F(input,init),ms=Math.round(performance.now()-t0);if(!r.ok)log('req',`${p} → HTTP ${r.status}`,{ms});else if(ms>SLOW_MS)log('slow',`${p} — ${(ms/1000).toFixed(1)} с`,{ms});return r;}
  catch(err){if(err?.name!=='AbortError')log('req',`${p} → ${err?.message||'нет связи'}`,{ms:Math.round(performance.now()-t0)});throw err;}};
 // ---- event stream: silences + reconnects; board facts from state events ----
 let lastMsg=0,gapOpen=0,maxGap=0;
 const ent={},norm=id=>String(id||'').replace(/^[^/-]+[/-]/,'').toLowerCase().replace(/ /g,'_');
 function board(d){const k=norm(d.id),v=d.value??d.state;ent[k]=v;const b=S.b;
  if(k==='uptime'&&Number.isFinite(+v)){const up=+v;if(b.up!=null&&up+30<b.up)log('boot',`Плата перезагрузилась${ent.reset_reason?' · '+ent.reset_reason:''}`);b.up=up;b.upAt=Date.now();}
  if(k==='reset_reason'&&v&&b.rr!==v){if(b.rr)log('boot',`Причина последнего сброса: ${v}`);b.rr=v;}
  if(k==='pulse_monitor'){const on=/^подключ|^connected/i.test(String(v??''));if(on&&!b.hrOn){b.hrSince=Date.now();if(b.hrOn===false)log('hr','Пульсометр подключился');}if(!on&&b.hrOn){const run=ent.treadmill_usage;if(run===true||/^(on|true|1)$/i.test(String(run)))log('hrdrop','Пульсометр пропал во время бега');else log('hr','Пульсометр отключился');}b.hrOn=on;}
  if(k==='heap_free'&&Number.isFinite(+v)){b.heap=+v;b.heapMin=Math.min(b.heapMin??+v,+v);}
  if(k==='heap_max_block'&&Number.isFinite(+v)){b.block=+v;b.blockMin=Math.min(b.blockMin??+v,+v);}
  if(k==='loop_time'&&Number.isFinite(+v)){b.loop=+v;b.loopMax=Math.max(b.loopMax??0,+v);if(+v>500)log('loop',`Цикл прошивки ${Math.round(+v)} мс`);}
  if(k==='esp32_wifi_signal'&&Number.isFinite(+v)){b.rssi=+v;b.rssiMin=Math.min(b.rssiMin??+v,+v);}
  save();}
 const ES=window.EventSource;
 if(ES){window.EventSource=function(url,cfg){const es=new ES(url,cfg);let opened=false;
   const seen=()=>{const now=Date.now();if(gapOpen){const g=now-gapOpen;maxGap=Math.max(maxGap,g);log('gap',`Нет данных от дорожки ${(g/1000).toFixed(0)} с`,{ms:g});gapOpen=0;}lastMsg=now;};
   es.addEventListener('open',()=>{if(opened)log('sse','Поток данных переподключился');opened=true;seen();});
   es.addEventListener('error',()=>{if(es.readyState===2)log('sse','Поток данных закрыт');});
   for(const ty of ['state','ping','log'])es.addEventListener(ty,e=>{seen();if(ty==='state')try{board(JSON.parse(e.data));}catch{}});
   return es;};
  window.EventSource.prototype=ES.prototype;Object.assign(window.EventSource,{CONNECTING:0,OPEN:1,CLOSED:2});}
 setInterval(()=>{if(lastMsg&&!gapOpen&&document.visibilityState==='visible'&&Date.now()-lastMsg>GAP_MS){gapOpen=lastMsg;emit();}},1000);
 document.addEventListener('visibilitychange',()=>{if(document.visibilityState==='visible'&&gapOpen){gapOpen=0;lastMsg=Date.now();}}); // a hidden tab is not a freeze
 // ---- page ----
 addEventListener('error',e=>log('js',`${e.message||'Ошибка'}${e.filename?' · '+e.filename.split('/').pop()+':'+e.lineno:''}`));
 addEventListener('unhandledrejection',e=>log('js',`Promise: ${e.reason?.message||e.reason}`));
 try{new PerformanceObserver(l=>{for(const x of l.getEntries())if(x.duration>=LONG_MS)log('long',`Страница подвисла на ${Math.round(x.duration)} мс`,{ms:Math.round(x.duration)});}).observe({type:'longtask',buffered:false});}catch{}
 log('open','Страница открыта · '+(navigator.userAgent.match(/(Chrome|Firefox|Safari|Edg)\/[\d]+/)?.[0]||'браузер'));
 const day=()=>{const from=Date.now()-864e5,ev=S.ev.filter(e=>e.t>=from),c=k=>ev.filter(e=>e.k===k).reduce((a,e)=>a+(e.n||1),0);
  return {boots:c('boot'),gaps:c('gap'),maxGap:Math.max(0,...ev.filter(e=>e.k==='gap').map(e=>e.ms||0)),sse:c('sse'),req:c('req'),slow:c('slow'),js:c('js'),long:c('long'),loop:c('loop'),hrdrop:c('hrdrop')};};
 return {log,day,board:()=>({...S.b,uptime:ent.uptime!=null?+ent.uptime:S.b.up,rr:ent.reset_reason??S.b.rr}),events:()=>S.ev.slice(),stalled:()=>!!gapOpen,
  clear(){S.ev=[];S.b={up:S.b.up,rr:S.b.rr};save();emit();},on(f){listeners.add(f);return()=>listeners.delete(f);},
  report(){const b=this.board(),d=day(),f=t=>new Date(t).toISOString().replace('T',' ').slice(0,19);return ['Treadmill diag '+f(Date.now())+' UTC',navigator.userAgent,'board: '+JSON.stringify(b),'24h: '+JSON.stringify(d),...S.ev.slice(-80).map(e=>`${f(e.t)} ${e.k}${e.n>1?' ×'+e.n:''} ${e.m}`)].join('\n');}};
})();
