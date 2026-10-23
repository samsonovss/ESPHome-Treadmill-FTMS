'use strict';
// Real-data adapter for the embedded concept page. Reads firmware entities only; commands go through
// TreadmillControls (same safety rules as before), nudges reuse the existing nudge buttons.
const ConceptLive={api({controls,t}){
 const ent=k=>controls.entities.get(k),val=k=>{const d=ent(k);return d?.value??d?.state;};
 const num=k=>{if(!controls.fresh())return null;const d=ent(k);if(!d||d.value===''||d.value==null)return null;const v=Number(d.value);return Number.isFinite(v)?v:null;};
 const pulseMode=p=>['Pulse Zone','Fat Burn','Recovery run'].includes(p),mmss=s=>Math.floor(s/60)+':'+String(Math.round(s%60)).padStart(2,'0');
 const names={None:'Быстрый старт','Pulse Zone':'Пульсовая зона','Fat Burn':'Жиросжигание','Recovery run':'Восстановительный бег',HIIT:'HIIT',Fartlek:'Фартлек'};
 // Firmware: zone k spans 50+10(k−1) … 60+10(k−1) % of HRmax (or of the reserve above resting HR, Karvonen).
 // HRmax = formula by age/sex or the measured value. "Selected Data" publishes the selected zone.
 function hrModel(){const age=num('number/age'),male=val('select/gender')!=='Female';if(age==null)return null;const formula=male?214-0.8*age:209-0.7*age,man=num('number/hr_max_manual'),
  max=val('select/hr_max_mode')==='Manual'&&man>=120&&man<=230?man:formula,rest=num('number/resting_heart_rate'),karv=val('select/hr_zone_method')==='Karvonen'&&rest>=30&&rest<max-20,base=karv?rest:0;
  return {max,formula,rest:karv?rest:null,at:p=>Math.round(base+(max-base)*p)};}
 function zones(){const m=String(val('text_sensor/selected_data')??'').match(/Zone\s*(\d)\D+(\d+)\D+(\d+)/);if(!m)return null;const z=+m[1],lo=+m[2],hi=+m[3];if(z<1||z>5||!(hi>lo))return null;
  // Firmware "HR Zones" (max=…;rest=…;b=50…100 %): single source of the bounds; formula below only for older firmware.
  {const b=String(val('text_sensor/hr_zones')??'').match(/b=([\d.,]+)/)?.[1]?.split(',').map(Number);if(b?.length===6&&b.every(Number.isFinite))return {z,target:[b[z-1],b[z]],inner:b.slice(1,5),b};}
  const h=hrModel();if(h)return {z,target:[lo,hi],inner:[.6,.7,.8,.9].map(h.at),b:[.5,.6,.7,.8,.9,1].map(h.at)};const max=lo/(.5+(z-1)*.1);return {z,target:[lo,hi],inner:[.6,.7,.8,.9].map(f=>Math.round(max*f))};}
 const RL=typeof RoutesLib!=='undefined'?RoutesLib:null,builtins=()=>typeof MAP_PROFILES!=='undefined'?MAP_PROFILES.routes:{};
 // "Custom route" on the belt → the library route whose name was uploaded last (fallback: raw active points).
 function customName(){const a=RL?.active();if(!a)return null;const b=builtins(),l0=RL.byName(b),lib={...l0,...RL.zwByName({...b,...l0})};return Object.keys(lib).find(n=>n.slice(0,60)===a.name)??null;}
 function allRoutes(){const b=builtins();if(!RL)return b;if(['treadmill_real_grade_at_maximum','treadmill_real_grade_at_zero','treadmill_maximum_incline_level','board_max_incline'].every(k=>num('number/'+k)!=null))RL.setCal({real:1,g0:num('number/treadmill_real_grade_at_zero')??0,gmax:num('number/treadmill_real_grade_at_maximum')??15,max:num('number/treadmill_maximum_incline_level')??15,mstep:(()=>{const bm=num('number/board_max_incline'),ml=num('number/treadmill_maximum_incline_level'),d=bm&&ml>=1&&bm>ml?bm/ml:1;return Math.max(0.1,Math.round(10/d)/10);})() /* incline step = board step (Motor board → Incline step), 12.10 */});if(!RL.loaded())RL.load();RL.zwLoad();const l0=RL.byName(b),lib={...l0,...RL.zwByName({...b,...l0})},a=RL.active();const out={...b,...lib};if(a&&!customName())out[a.name+' (на дорожке)']={points:a.points,endKm:a.points.at(-1)[0],active:true};return out;}
 function route(){const m=controls.fresh()?val('select/map'):null;if(m===RL?.CUSTOM){RL.refreshActive();const n=customName(),a=RL.active();const r=n?allRoutes()[n]:null;return a?{name:n??a.name,points:a.points,geom:r?.geom??null,detail:r?.detail}:null;}
  const r=m&&m!=='None'?builtins()[m]:null;return r?{name:m,points:r.points}:null;}
 let latch=null;
 const nudgeBtn=(k,dir)=>document.getElementById('nudge-'+k+'-'+(dir>0?'plus':'minus'));
 return {
  snapshot(){const fresh=controls.fresh(),p=fresh?val('select/workout_program'):null,r=route(),z=zones(),st0=controls.state(),running=st0==='running'||st0==='paused';
   // Firmware overwrites Set Warm-up/Run/Cool-down with REMAINING minutes while running, so latch the plan while stopped.
   const raw=k=>num('number/'+k)??0,t0=num('sensor/treadmill_time')??0,cur={'set_warm-up_time':raw('set_warm-up_time'),set_run_time:raw('set_run_time'),'set_cool-down_time':raw('set_cool-down_time'),hiit_cycles:raw('hiit_cycles')};
   if(!running||latch?.p!==p){latch=running?{p,v:{...cur,set_run_time:cur.set_run_time+t0/60}}:fresh?{p,v:cur}:null;
    /* 17.10: the firmware counts cycles / minutes DOWN while running — a page opened (or reloaded, iPad) mid-run must not take those as the plan: keep the plan latched at the start in localStorage */
    const at=Date.now()-t0*1000;try{if(running){const sv=JSON.parse(localStorage.getItem('tm.plan.latch')||'null');if(sv&&sv.p===p&&Math.abs(sv.at-at)<15000)latch.v={...latch.v,...sv.v};}else if(latch)localStorage.setItem('tm.plan.latch',JSON.stringify({p,at:Date.now(),v:latch.v}));}catch{}}
   else if(running&&t0<3){try{localStorage.setItem('tm.plan.latch',JSON.stringify({p,at:Date.now()-t0*1000,v:latch.v}));}catch{}}
   const n=k=>k in cur?(running&&latch?Math.max(latch.v[k],cur[k]):cur[k]):raw(k),warm=n('set_warm-up_time')*60,cool=n('set_cool-down_time')*60;
   // Route + pulse program = map workout (incline by map, speed by pulse): the route decides the layout.
   const byPulse=pulseMode(p);let mode=p==='HIIT'?'hiit':p==='Fartlek'?'fartlek':r?'map':byPulse?'pulse':'manual',total=null,hiit=null,sub='';
   if(mode==='hiit'){hiit={warm,hi:n('hiit_high_duration'),lo:n('hiit_low_duration'),cycles:n('hiit_cycles'),cool,low:n('hiit_low_speed'),high:n('hiit_high_speed'),manual:val('select/hiit_work_mode')==='Manual'};if(hiit.hi&&hiit.lo&&hiit.cycles){total=warm+hiit.cycles*(hiit.hi+hiit.lo)+cool;sub=`${hiit.cycles} × (${mmss(hiit.hi)} / ${mmss(hiit.lo)}) · работа ${hiit.manual?hiit.high+' км/ч':'по пульсу'} · отдых ${hiit.low} км/ч`;}else hiit=null;}
   else if(mode==='fartlek'){const g=String(val('text/fartlek_plan')??'').split(',').map(x=>x.split('/').map(Number)).filter(a=>a[0]>0&&a[1]>0);total=g.reduce((s,a)=>s+a[0],0)||null;sub=g.length?`${g.length} отрезков · ${Math.min(...g.map(a=>a[1]))}–${Math.max(...g.map(a=>a[1]))} км/ч`:'План не задан';}
   else if(mode!=='map'){const run=n('set_run_time')*60;if(run)total=warm+run+(byPulse?cool:0);}   // route: length is in km — no time total (was 1:06:00 from the pulse program's Run Time)
   if(mode==='pulse')sub=z?`Цель Z${z.z} · ${z.target[0]}–${z.target[1]} · скорость подбирает дорожка`:'Цель: нет данных Selected Data';
   if(mode==='map')sub='Наклон по карте · '+(byPulse?(z?`скорость по пульсу · Z${z.z} · ${z.target[0]}–${z.target[1]}`:'скорость по пульсу'):'скорость вручную');if(mode==='manual')sub=fresh?'Ручное управление':'Нет связи с дорожкой';
   const name=(p?names[p]??p:'—')+(r&&mode!=='map'?' · '+r.name:mode==='map'?' · '+r.name:'');
   // Real phase from firmware "Status" text sensor (remaining time is published by the firmware).
   const st=String(val('text_sensor/status')??''),rem=x=>x?(+x[1])*60+(+x[2]):null;let phase=null,prem=null,wait=null,cycLeft=null;
   if(/^Warm-up/.test(st)){phase='warm';prem=rem(st.match(/\((\d+):(\d+)\)/));const w=st.match(/Need (\d+)/);wait=/Wait pulse/.test(st)?'pulse':w?+w[1]:null;if(/Done/.test(st)){phase='run';prem=null;}}
   else if(/^Cool-down/.test(st)){phase='cool';prem=rem(st.match(/\((\d+):(\d+)\)/));if(/Done/.test(st))phase='done';}
   else if(/^(HIIT )?High |^Recovery /.test(st)){phase=/^Recovery /.test(st)?'rest':'work';prem=rem(st.match(/\((\d+):(\d+)\)\s*$/));const c=st.match(/C:(\d+)/);cycLeft=c?+c[1]:null;}
   else if(/^Fartlek \d+\/\d+ /.test(st)){phase=/ Fast /.test(st)?'work':'rest';prem=rem(st.match(/\((\d+):(\d+)\)\s*$/));}
   else if(/^Fartlek: done/.test(st))phase='done';
   else if(/^HIIT/.test(st))phase='hiit';
   else if(/^Program: Done/.test(st))phase='done';
   else if(/(^|, )Z\d: .*\((\d+):(\d+)\)\s*$/.test(st)){phase='run';prem=rem(st.match(/\((\d+):(\d+)\)\s*$/));}
   const plan={warm,run:n('set_run_time')*60,cool};
   const hr=num('sensor/heart_rate');
   const notice=controls.notice?t(controls.notice):'';
   // warm-up pulse threshold (firmware: base + (max − base) × Warm-up Target HR %) from the HR Zones bounds b (50…100 %)
   const wuHr=(()=>{const b=String(val('text_sensor/hr_zones')??'').match(/b=([\d.,]+)/)?.[1]?.split(',').map(Number),pc=num('number/warm-up_target_hr')??60;return b?.length===6&&b.every(Number.isFinite)?Math.round(b[0]+(b[5]-b[0])*(pc-50)/50):null;})();
   return {wuHr,routeOff:num('sensor/route_km_offset')??0,warmUp:phase==='warm',state:controls.state()==='unknown'?'stopped':controls.state(),fresh,mode,byPulse,name,sub,total,zones:z,route:r?.points??null,routeGeom:r?.geom??null,motorTemp:num('sensor/motor'),minSave:num('number/min_workout_to_save'),freeRunAvail:!!ent('switch/free_running_mode'),freeRun:controls.fresh()&&[true,'ON'].includes(val('switch/free_running_mode')),beltPos:num('sensor/my_distance'),belt:{L:num('number/tm_length'),A:num('number/tm_accel'),D:num('number/tm_decel')},hrLink:controls.fresh()?val('text_sensor/pulse_monitor'):null,appLink:controls.fresh()?val('text_sensor/zwift'):null,btModel:controls.fresh()?val('text_sensor/bt_model'):null,ftmsStatus:controls.fresh()?val('text_sensor/ftms_status'):null,overheatLimit:num('number/overheat_stop_temperature'),routeDetail:r?.detail??null,routeName:r?.name??null,hiit,notice,status:st,phase,prem,wait,cycLeft,plan,
    sample:{t:num('sensor/treadmill_time'),hr:hr>0?hr:null,spd:num('sensor/treadmill_speed')??num('sensor/treadmill_speed_motor'),inc:num('sensor/treadmill_incline'),dist:num('sensor/treadmill_distance'),cal:num('sensor/calories_burned')}};},
  programs:(()=>{const hc=typeof TreadmillHistory!=='undefined'?new TreadmillHistory.Client():null;return {
   cache:new Map(),rev:()=>num('sensor/programs_revision'),
   async list(){const l=JSON.parse(await hc.request('/api/programs/list',false)),ids=(l.items||[]).map(x=>x.id),out=[];for(const k of [...this.cache.keys()])if(!ids.includes(k))this.cache.delete(k);for(const id of ids){if(!this.cache.has(id)){try{this.cache.set(id,JSON.parse(await hc.request('/api/programs/file?id='+id,false)));}catch{continue;}}out.push(this.cache.get(id));}return out;},
   async remove(id){await hc.request('/api/programs/delete?id='+encodeURIComponent(id),false,'POST');this.cache.delete(id);}};})(),
  presets:(()=>{const hc=typeof TreadmillHistory!=='undefined'?new TreadmillHistory.Client():null;return {
   async get(kind){const d=JSON.parse(await hc.request('/api/presets?kind='+kind,false));if(!d||!Array.isArray(d.items))throw Error('presets');return d.items;},
   async save(kind,items){const r=await hc.fetcher('/api/presets?kind='+kind,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({items})});if(!r.ok)throw Error('presetsSave');}};})(),
  history:(()=>{const hc=typeof TreadmillHistory!=='undefined'?new TreadmillHistory.Client():null,num=v=>v===''||v==null?null:(Number.isFinite(+v)?+v:null);
   const parse=text=>{const lines=text.trim().split(/\r?\n/),head=lines.shift().split(','),ix=k=>head.indexOf(k);const keep=['status','incline_level','incline_pct','target_zone'].map(ix),prev=[];/* compact CSV: empty = previous value */return lines.map(l=>{const c=l.split(',');for(const i of keep)if(i>=0){if(c[i]==='')c[i]=prev[i]??'';else prev[i]=c[i];}return c;}).filter(c=>c[ix('status')]!=='paused').map(c=>({t:num(c[ix('elapsed_s')])??num(c[ix('time_s')]),hr:(v=>v>0?v:null)(num(c[ix('heart_rate')])),spd:num(c[ix('speed_kmh')]),inc:num(c[ix('incline_level')])??num(c[ix('incline_pct')]),dist:num(c[ix('distance_km')]),cal:num(c[ix('calories')]),tz:num(c[ix('target_zone')])})).filter(r=>r.t!=null);};
   return {
    meta:new Map(),
    // summaries=1 (new firmware) embeds each summary JSON in the list: one request instead of one per workout.
    async ids(){if(!hc)return [];await hc.check();const out=[];let after='';for(let i=0;i<50;i++){const page=JSON.parse(await hc.request('/workouts/list?'+(after?'after='+after+'&':'')+'limit=20&summaries=1',false));if(!page||!Array.isArray(page.items)||page.items.some(x=>!TreadmillHistory.validId(x.id)))throw Error('historyInvalid');for(const x of page.items){out.push(x.id);if(x.summary&&typeof x.summary==='object')this.meta.set(x.id,x.summary);}if(!page.next||page.next===after)break;after=page.next;}return out.reverse();},
    async summary(id){return JSON.parse(await hc.file(id,'json'));},
    async rows(id){return parse(await hc.file(id,'csv'));},
    async download(id,type){let content,mime='text/plain';if(type==='fit'){const sm=JSON.parse(await hc.file(id,'json'));content=TreadmillFit.build(TreadmillFit.parseCSV(await hc.file(id,'csv'),sm.interrupted===true),sm);mime='application/octet-stream';}else{/* compact CSV on the board → full CSV for the user (every cell filled) */const raw=await hc.file(id,'csv'),rows=TreadmillFit.parseCSV(raw),head=raw.slice(0,raw.indexOf('\n')).split(','),q=v=>/[",\n]/.test(v)?'"'+String(v).replace(/"/g,'""')+'"':v;content=head.join(',')+'\n'+rows.map(r=>head.map(k=>q(r[k]??'')).join(',')).join('\n')+'\n';mime='text/csv';}const url=URL.createObjectURL(new Blob([content],{type:mime})),a=document.createElement('a');a.href=url;a.download=id+'.'+type;document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);},
    // Delete for good (12.10): move to the trash, then purge the files. Old firmware has no purge — then it just stays in the trash.
    async trash(id){await this.op(id,'purge');},   // delete = purge in one step (12.10)
    // The recorder answers 500 "archive_busy" for ~1–3 s after each job: retry instead of failing a bulk delete (12.10).
    async idle(){for(let i=0;i<60;i++){try{const s=JSON.parse(await hc.request('/workouts/status',false));if(s&&s.state==='idle'&&!s.recording)return;}catch{}await new Promise(r=>setTimeout(r,500));}},
    // wait until the recorder is idle before each request — no 409/500 "archive_busy" storm in the log (12.10)
    async op(id,operation){let last;for(let i=0;i<10;i++){await this.idle();try{return await hc.mutate(id,operation);}catch(e){last=e;if(e.message==='historyOperationFailed')throw e;await new Promise(r=>setTimeout(r,2000));}}throw last;},
    // Workouts hidden by the old "soft" delete: list the trash and purge each (12.10).
    async trashIds(){if(!hc)return [];await hc.check();const out=[];let after='';for(let i=0;i<50;i++){let page;for(let k=0;;k++){try{page=JSON.parse(await hc.request('/api/workouts/trash/list?'+(after?'after='+after+'&':'')+'limit=20',false));break;}catch(e){if(k>=15)throw e;await new Promise(r=>setTimeout(r,700));}}const it=Array.isArray(page?.items)?page.items:[];for(const x of it)if(TreadmillHistory.validId(x.id))out.push(x.id);if(!page.next||!it.length)break;after=page.next;}return out;},
    async purge(id){await this.op(id,'purge');},
    async setMinSave(m){const r=await controls.send('/number/'+encodeURIComponent('Min Workout To Save')+'/set?value='+m);if(!r.ok)throw Error('rejected');},
    minSaveSec(){const d=ent('number/min_workout_to_save');const v=Number(d?.value??d?.state);return Number.isFinite(v)?v*60:120;}};})(),
  // Program setup: current firmware values + traits; apply writes only while stopped, never starts the belt.
  settings(){const tr=k=>{const d=ent(k);if(!d)return null;const r=[d.min_value,d.max_value,d.step].map(Number);return r.every(Number.isFinite)&&r[2]>0?{min:r[0],max:r[1],step:r[2]}:null;},opts=k=>{const d=ent(k),o=d?.option??d?.options;return Array.isArray(o)?o:[];},v=k=>{const x=val(k);return x==null?null:x;};
   const keys={run:'number/set_run_time',warm:'number/set_warm-up_time',cool:'number/set_cool-down_time',rwarm:'number/set_warm-up_time',rcool:'number/set_cool-down_time',hi:'number/hiit_high_duration',lo:'number/hiit_low_duration',low:'number/hiit_low_speed',high:'number/hiit_high_speed',cycles:'number/hiit_cycles'},out={fplan:v('text/fartlek_plan'),fname:v('text/fartlek_name'),program:v('select/workout_program'),zone:v('select/heart_rate_zone'),map:(m=>m===RL?.CUSTOM?(RL.refreshActive(),customName()??m):m)(v('select/map')),programs:opts('select/workout_program'),zones:opts('select/heart_rate_zone'),maps:opts('select/map'),hiitMode:v('select/hiit_work_mode'),rwMode:v('select/warm-up_mode'),rcMode:v('select/cool-down_mode'),hiitModes:opts('select/hiit_work_mode'),traits:{},keys};
   for(const [k,key]of Object.entries(keys)){out[k]=num(key);out.traits[k]=tr(key);}return out;},
  canApply:()=>controls.fresh()&&controls.state()==='stopped'&&!controls.pending.size,
  async apply(list){if(!controls.fresh()||controls.state()!=='stopped')throw Error('Дорожка должна стоять');for(let [key,value]of list){if(key==='select/map'&&RL&&value!=='None'&&!builtins()[value]){const r=allRoutes()[value];if(r?.cat){await RL.upload({name:value,geom:await RL.zwGeom(r.cat),detail:10,intensity:1});}else{if(!r?.custom)throw Error('Маршрут не найден');await RL.upload({...RL.get(r.custom),name:value});}value=RL.CUSTOM;}const d=ent(key);if(!d)throw Error('нет сущности '+key);const path='/'+d.domain+'/'+encodeURIComponent(d.name)+'/set?'+(d.domain==='select'?'option=':'value=')+encodeURIComponent(value);const r=await controls.send(path);if(!r.ok)throw Error('HTTP '+(r.status??''));await new Promise(res=>setTimeout(res,60));}},
  // Whole current workout from the recorder's PSRAM buffer (new firmware); null on old firmware.
  async live(){const rows=[];let from=0,meta=null;for(let i=0;i<20;i++){const c=new AbortController(),tm=setTimeout(()=>c.abort(),10000);let j;try{const r=await fetch('/api/workouts/live?from='+from,{cache:'no-store',signal:c.signal});if(!r.ok)return null;j=await r.json();}catch{return null;}finally{clearTimeout(tm);}
    meta=j;if(!Array.isArray(j.rows))return null;for(const x of j.rows)if(Array.isArray(x)&&!x[7])rows.push({t:x[1],hr:x[2]>0?x[2]:null,spd:x[3],inc:x[4],dist:x[5],cal:x[6]});from+=j.rows.length;if(!j.rows.length||from>=j.total)break;}return {id:meta?.id,active:!!meta?.active,rows};},
  routes:allRoutes,routesLib:RL,
  can:k=>controls.allowed(k),command:k=>controls.command(k),
  // RunFree: firmware switch remembers the previous mode (e.g. pulse) and restores it when turned off; from STOP it starts free running.
  async freeRun(on){const d=ent('switch/free_running_mode');if(!d||!controls.fresh()||controls.state()==='paused')return false;try{/* 16.10: from STOP the switch alone doesn't move the belt (firmware marks Free Run active before its start block) — start like ▶ first, then switch on (Nextion does switch + start_manual) */if(on&&controls.state()==='stopped'){if(!await controls.command('start'))return false;await new Promise(r=>setTimeout(r,1500));}const r=await controls.send('/switch/'+encodeURIComponent(d.name)+(on?'/turn_on':'/turn_off'));return !!r.ok;}catch{return false;}},
  canNudge:k=>{const b=nudgeBtn(k,1);return !!b&&!b.disabled;},nudge:(k,dir)=>{const b=nudgeBtn(k,dir);if(b&&!b.disabled)b.click();}};
}};
