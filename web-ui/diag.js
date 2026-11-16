'use strict';
// Diagnostics journal (09.10): what went wrong and when — kept in this browser only (localStorage ring buffer).
// Browser side: SSE silences and reconnects, failed/slow requests, script errors, long frames.
// Board side (from existing entities): reboots (Uptime went back), Reset Reason, min free heap, loop time, Wi-Fi.
const TMDiag=(()=>{
 const KEY='tm.diag.v1',MAX=200,GAP_MS=12000,SLOW_MS=3000,LONG_MS=200,MEM_FREE=24576,MEM_BLOCK=8192,MEM_MS=60000;
 let S;try{S=JSON.parse(localStorage.getItem(KEY))||{};}catch{S={};}
 S.ev=Array.isArray(S.ev)?S.ev:[];S.b=S.b||{};
 let saveT=0;const save=()=>{clearTimeout(saveT);saveT=setTimeout(()=>{try{localStorage.setItem(KEY,JSON.stringify(S));}catch{}},500);};
 // Sustained-low detection is session-local, never resumed from cached timestamps.
 S.b.lowSince=null;S.b.lowLogged=false;
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
 // Only current SSE observations enter live cards/chart; persisted board facts remain report-only.
 const started=Date.now(),STALE_MS=150000,HISTORY_MAX=1200;
 let lastMsg=0,gapOpen=0,maxGap=0,connected=false,segment=0;
 const live={},at={},history=[];
 const fields={workout_storage_total:'storageTotal',workout_storage_used:'storageUsed',workout_storage_free:'storageFree',heap_free:'heap',heap_max_block:'block',free_psram:'psram',esp32_wifi_signal:'rssi',uptime:'uptime',loop_time:'loop',memory_drop:'drop',memory_trend:'trend',reset_reason:'rr'};
 const numeric=v=>v!==null&&v!==undefined&&String(v).trim()!==''&&Number.isFinite(Number(v));
 function interrupt(){connected=false;segment++;for(const k of Object.keys(live))delete live[k];for(const k of Object.keys(at))delete at[k];S.b.lowSince=null;S.b.lowLogged=false;}
 function snapshot(){const now=Date.now(),out={};for(const k of Object.keys(live))if(connected&&(k==='trend'||k==='rr'||k==='drop'||now-at[k]<=STALE_MS))out[k]=live[k];return out;}
 function status(){const b=snapshot();if(!connected)return lastMsg?'disconnected':'pending';if(S.b.lowLogged&&b.heap!=null&&b.block!=null)return 'low';if(b.trend==='decreasing')return 'warning';return ['heap','block','psram','rssi','uptime'].some(k=>b[k]==null)?'pending':'ok';}

 const ent={},norm=id=>String(id||'').replace(/^[^/-]+[/-]/,'').toLowerCase().replace(/ /g,'_');
 function board(d){const k=norm(d.id),v=d.value??d.state;ent[k]=v;const b=S.b,now=Date.now();
  if(k==='uptime'&&numeric(v)&&live.uptime!=null&&+v+30<live.uptime){interrupt();connected=true;}
  const field=fields[k];if(field){if(field==='trend'||field==='rr'){live[field]=String(v??'');at[field]=now;}else if(numeric(v)){live[field]=+v;at[field]=now;}}
  if((k==='heap_free'||k==='heap_max_block')&&numeric(v)){history.push({t:now,k:field,v:+v,segment});if(history.length>HISTORY_MAX)history.splice(0,history.length-HISTORY_MAX);}

  if(k==='uptime'&&numeric(v)){const up=+v;if(b.up!=null&&up+30<b.up)log('boot',`Плата перезагрузилась${ent.reset_reason?' · '+ent.reset_reason:''}`);b.up=up;b.upAt=Date.now();}
  if(k==='reset_reason'&&v&&b.rr!==v){if(b.rr)log('boot',`Причина последнего сброса: ${v}`);b.rr=v;}
  if(k==='pulse_monitor'){const on=/^подключ|^connected/i.test(String(v??''));if(on&&!b.hrOn){b.hrSince=Date.now();if(b.hrOn===false)log('hr','Пульсометр подключился');}if(!on&&b.hrOn){const run=ent.treadmill_usage;if(run===true||/^(on|true|1)$/i.test(String(run)))log('hrdrop','Пульсометр пропал во время бега');else log('hr','Пульсометр отключился');}b.hrOn=on;}
  if(k==='heap_free'&&numeric(v)){b.heap=+v;b.heapMin=Math.min(b.heapMin??+v,+v);}
  if(k==='heap_max_block'&&numeric(v)){b.block=+v;b.blockMin=Math.min(b.blockMin??+v,+v);}
  // Short dips (HTTPS update check, page download, OTA) are normal; only memory that stays low for a minute counts.
  if((k==='heap_free'||k==='heap_max_block')&&snapshot().heap!=null&&snapshot().block!=null){const low=live.heap<MEM_FREE||live.block<MEM_BLOCK,now=Date.now();
   if(!low){b.lowSince=null;b.lowLogged=false;}else{b.lowSince??=now;if(!b.lowLogged&&now-b.lowSince>=MEM_MS){b.lowLogged=true;log('mem',`Мало памяти дольше минуты: свободно ${Math.round(b.heap/1024)} КБ, блок ${Math.round(b.block/1024)} КБ`);}}}
  if(k==='loop_time'&&numeric(v)){b.loop=+v;b.loopMax=Math.max(b.loopMax??0,+v);if(+v>500)log('loop',`Цикл прошивки ${Math.round(+v)} мс`);}
  if(k==='memory_trend'&&v){if(v==='decreasing'&&b.trend!=='decreasing')log('leak',`Свободная память уменьшается: −${ent.memory_drop??'?'} КБ с запуска`);b.trend=String(v);}
  if(k==='memory_drop'&&numeric(v))b.drop=+v;
  if(k==='free_psram'&&numeric(v))b.psram=+v;
  if(k==='esp32_wifi_signal'&&numeric(v)){b.rssi=+v;b.rssiMin=Math.min(b.rssiMin??+v,+v);}
  save();emit();}
 const ES=window.EventSource;
 if(ES){window.EventSource=function(url,cfg){const es=new ES(url,cfg);let opened=false;
   const seen=()=>{const now=Date.now();if(lastMsg&&now-lastMsg>GAP_MS&&!gapOpen){gapOpen=lastMsg;interrupt();}if(gapOpen){const g=now-gapOpen;maxGap=Math.max(maxGap,g);log('gap',`Нет данных от дорожки ${(g/1000).toFixed(0)} с`,{ms:g});gapOpen=0;}lastMsg=now;connected=true;};
   es.addEventListener('open',()=>{if(opened)log('sse','Поток данных переподключился');opened=true;seen();});
   es.addEventListener('error',()=>{if(lastMsg&&!gapOpen)gapOpen=lastMsg;interrupt();emit();if(es.readyState===2)log('sse','Поток данных закрыт');});
   for(const ty of ['state','ping','log'])es.addEventListener(ty,e=>{seen();if(ty==='state')try{board(JSON.parse(e.data));}catch{}});
   return es;};
  window.EventSource.prototype=ES.prototype;Object.assign(window.EventSource,{CONNECTING:0,OPEN:1,CLOSED:2});}
 setInterval(()=>{if(lastMsg&&!gapOpen&&Date.now()-lastMsg>GAP_MS){gapOpen=lastMsg;interrupt();emit();}},1000);
 document.addEventListener('visibilitychange',()=>{if(document.visibilityState==='visible'&&lastMsg&&Date.now()-lastMsg>GAP_MS){interrupt();emit();}}); // a hidden tab is not a freeze
 // ---- page ----
 addEventListener('error',e=>log('js',`${e.message||'Ошибка'}${e.filename?' · '+e.filename.split('/').pop()+':'+e.lineno:''}`));
 addEventListener('unhandledrejection',e=>log('js',`Promise: ${e.reason?.message||e.reason}`));
 try{new PerformanceObserver(l=>{for(const x of l.getEntries())if(x.duration>=LONG_MS)log('long',`Страница подвисла на ${Math.round(x.duration)} мс`,{ms:Math.round(x.duration)});}).observe({type:'longtask',buffered:false});}catch{}
 log('open','Страница открыта · '+(navigator.userAgent.match(/(Chrome|Firefox|Safari|Edg)\/[\d]+/)?.[0]||'браузер'));
 const day=()=>{const from=Date.now()-864e5,ev=S.ev.filter(e=>e.t>=from),c=k=>ev.filter(e=>e.k===k).reduce((a,e)=>a+(e.n||1),0);
  return {boots:c('boot'),gaps:c('gap'),maxGap:Math.max(0,...ev.filter(e=>e.k==='gap').map(e=>e.ms||0)),sse:c('sse'),req:c('req'),slow:c('slow'),js:c('js'),long:c('long'),loop:c('loop'),hrdrop:c('hrdrop'),mem:c('mem')};};
 return {log,day,live:snapshot,status,history:()=>history.map(p=>({...p})),started,board:()=>({...S.b,uptime:ent.uptime!=null?+ent.uptime:S.b.up,rr:ent.reset_reason??S.b.rr}),events:()=>S.ev.slice(),stalled:()=>!!gapOpen,
  clear(){S.ev=[];S.b={...S.b,heapMin:S.b.heap,blockMin:S.b.block,loopMax:S.b.loop,rssiMin:S.b.rssi};save();emit();},on(f){listeners.add(f);return()=>listeners.delete(f);},
  report(){const b=this.board(),d=day(),f=t=>new Date(t).toISOString().replace('T',' ').slice(0,19);return ['Treadmill diag '+f(Date.now())+' UTC',navigator.userAgent,'board: '+JSON.stringify(b),'live: '+JSON.stringify(snapshot()),'status: '+status(),'chart: browser-only, since '+f(started)+' UTC; '+history.length+' observations','24h: '+JSON.stringify(d),...S.ev.slice(-80).map(e=>`${f(e.t)} ${e.k}${e.n>1?' ×'+e.n:''} ${e.m}`)].join('\n');}};
})();
