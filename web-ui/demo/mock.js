'use strict';
// DEMO ONLY: a virtual treadmill in the browser for the GitHub Pages demo. It replaces EventSource('/events')
// and same-origin fetch, so the real page runs unchanged without a board. generate.py injects it only into
// generated/demo.html; the firmware page never contains it. Entity names, status strings and API answers
// copy the firmware (packages/*.yaml, treadmill_recorder.h) so the page takes the same code paths.
(()=>{
const SEED=/*DEMO_ENTITIES*/[],VERSION=/*DEMO_VERSION*/'',SAMPLE_ROUTES=/*DEMO_ROUTES*/{};
const E=new Map(),DEC=new Map(),UNIT=new Map(),listeners=new Set();
for(const e0 of SEED){const i=e0.id.indexOf('/'),e={domain:e0.id.slice(0,i),name:e0.id.slice(i+1),...e0};E.set(e.id,e);
 const m=String(e.state??'').match(/^(-?\d+)(?:\.(\d+))?\s*(.*)$/);DEC.set(e.id,m?(m[2]||'').length:0);UNIT.set(e.id,m?m[3]:'');}
const ent=id=>E.get(id),num=id=>{const v=Number(ent(id)?.value);return Number.isFinite(v)?v:0;},str=id=>String(ent(id)?.value??'');
const emit=e=>{const data=JSON.stringify(e);for(const l of listeners)l('state',data);};
function set(id,v){const e=ent(id);if(!e)return;let value=v,state;
 if(e.domain==='switch'||e.domain==='binary_sensor'){value=!!v;state=v?'ON':'OFF';}
 else if(typeof v==='number'){const d=DEC.get(id),s=v.toFixed(d),u=UNIT.get(id);state=s+(u?' '+u:'');value=e.domain==='number'?s:+s;}
 else state=value=String(v);
 if(e.value===value&&e.state===state)return;e.value=value;e.state=state;emit(e);}
const mmss=s=>Math.floor(s/60)+':'+String(Math.floor(s%60)).padStart(2,'0');
let rnd=(a=>()=>{a|=0;a=a+0x6D2B79F5|0;let t=Math.imul(a^a>>>15,1|a);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296;})(20261023);

// --- heart-rate zones exactly like the firmware ("HR Zones", "Selected Data") ---
function hrModel(){const age=num('number/Age'),male=str('select/Gender')!=='Female',f=Math.round(male?214-0.8*age:209-0.7*age),man=num('number/HR Max Manual');
 const max=str('select/HR Max Mode')==='Manual'?man:f,rest=num('number/Resting Heart Rate'),karv=str('select/HR Zone Method')==='Karvonen',base=karv?rest:0;
 const b=[.5,.6,.7,.8,.9,1].map(p=>Math.round(base+(max-base)*p));return {max,rest:karv?rest:0,b};}
function pubZones(){const h=hrModel(),z=+str('select/Heart Rate Zone')||2;set('text_sensor/HR Zones',`max=${h.max};rest=${h.rest};b=${h.b.join(',')}`);
 set('text_sensor/Selected Data',`${str('select/Gender')} ${num('number/Age')}, HR Zone ${z}: ${h.b[z-1]}–${h.b[z]} bpm`);}
const zoneOf=(hr,b)=>{if(!(hr>=b[0]))return 0;for(let z=5;z>=1;z--)if(hr>=b[z-1])return z;return 0;};

// --- workout plan: the same segments and status texts the firmware publishes ---
const PULSE=['Pulse Zone','Fat Burn','Recovery run'];
function plan(cfg){const g=[],warm=cfg.warm*60,cool=cfg.cool*60,p=cfg.program;
 if(p==='HIIT'){if(warm)g.push({k:'warm',d:warm});for(let c=0;c<cfg.cycles;c++){g.push({k:'work',d:cfg.hi,c:cfg.cycles-c});g.push({k:'rest',d:cfg.lo,c:cfg.cycles-c});}if(cool)g.push({k:'cool',d:cool});}
 else if(p==='Fartlek'){const s=cfg.fplan.split(',').map(x=>x.split('/')).filter(a=>+a[0]>0);if(warm)g.push({k:'warm',d:warm});s.forEach((a,i)=>{const z=/^z(\d)$/i.exec(a[1]);g.push({k:'fk',d:+a[0],s:z?null:+a[1],z:z?+z[1]:0,i:a[2]!=null?+a[2]:null,n:i+1,N:s.length});});if(cool)g.push({k:'cool',d:cool});}
 else if(PULSE.includes(p)){if(warm)g.push({k:'warm',d:warm});g.push({k:'run',d:Math.max(1,cfg.run)*60,z:cfg.zone});if(cool)g.push({k:'cool',d:cool});}
 else{if(warm)g.push({k:'warm',d:warm});g.push({k:'man',d:cfg.run>0?cfg.run*60:Infinity});if(cool&&cfg.run>0)g.push({k:'cool',d:cool});}   // quick start: warm-up, then manual
 let t=0;for(const s of g){s.a=t;t+=s.d;}return g;}
function cfgNow(){return {program:str('select/Workout Program'),warm:num('number/Set Warm-up Time'),cool:num('number/Set Cool-down Time'),run:num('number/Set Run Time'),zone:+str('select/Heart Rate Zone')||2,
 hi:num('number/HIIT High Duration'),lo:num('number/HIIT Low Duration'),cycles:num('number/HIIT Cycles'),high:num('number/HIIT High Speed'),low:num('number/HIIT Low Speed'),hiitManual:str('select/HIIT Work Mode')==='Manual',
 fplan:str('text/Fartlek Plan'),wStart:num('number/Warm-up Start Speed'),wEnd:num('number/Warm-up End Speed'),cMin:num('number/Cool-down Min Speed'),adj:num('number/Pulse Adjust Interval')||4,
 weight:num('number/weight')||72,maxLvl:num('number/Treadmill Maximum Incline Level')||15,g0:num('number/Treadmill Real Grade At Zero'),gmax:num('number/Treadmill Real Grade At Maximum')||15};}

// One simulated second. x = session state, cfg = settings at the start, manual = live Set Speed/Incline (or null).
function step(x,cfg,manual){const h=x.hr0??hrModel(),seg=x.plan.find(s=>x.t>=s.a&&x.t<s.a+s.d),left=seg?seg.a+seg.d-x.t:0;let tgt=x.tgt??3,inc=x.inc??0,st='';
 const reg=(z)=>{const lo=h.b[z-1],hi=h.b[z];if(x.t%cfg.adj===0){if(x.hr<lo-3)tgt+=x.hr<lo-12?0.5:0.2;else if(x.hr>hi+2)tgt-=x.hr>hi+10?0.5:0.2;}return [lo,hi];};
 if(!seg){if(x.plan.length){x.done=true;st=cfg.program==='Fartlek'?'Fartlek: done':'Program: Done';}else{tgt=manual?.spd??tgt;inc=manual?.inc??inc;}}
 else if(seg.k==='warm'){tgt=Math.min(cfg.wEnd,cfg.wStart+(cfg.wEnd-cfg.wStart)*(x.t-seg.a)/Math.max(1,seg.d));inc=0;st=`Warm-up: (${mmss(left)})`;}
 else if(seg.k==='cool'){tgt=Math.max(cfg.cMin,(x.tgt??5)-(x.t%15===0?0.5:0));inc=0;st=`Cool-down: (${mmss(left)})`;}
 else if(seg.k==='run'){const [lo,hi]=reg(seg.z);st=`Z${seg.z}: ${lo}-${hi} bpm (${mmss(left)})`;inc=manual?.inc??inc;}
 else if(seg.k==='work'){if(cfg.hiitManual){tgt=cfg.high;st=`High ${cfg.high.toFixed(1)} km/h, C:${seg.c} (${mmss(left)})`;}else{const [lo,hi]=reg(cfg.zone);st=`High Z${cfg.zone} ${lo}-${hi}, C:${seg.c} (${mmss(left)})`;}}
 else if(seg.k==='rest'){tgt=cfg.low;st=`Recovery ${cfg.low.toFixed(1)} km/h, C:${seg.c} (${mmss(left)})`;}
 else if(seg.k==='fk'){if(seg.z)reg(seg.z);else tgt=seg.s;if(seg.i!=null)inc=seg.i;const fast=seg.z?seg.z>=3:seg.s>=8;st=`Fartlek ${seg.n}/${seg.N} ${fast?'Fast':'Easy'} ${seg.z?'Z'+seg.z:seg.s.toFixed(1)} (${mmss(left)})`;}
 else if(seg.k==='man'){tgt=manual?.spd??tgt;inc=manual?.inc??inc;}
 if(x.route&&manual?.route)inc=routeLevel(manual.route,x.dist);
 tgt=Math.max(0.6,Math.min(16,Math.round(tgt*10)/10));x.tgt=tgt;x.inc=inc;
 x.spd=x.spd<tgt?Math.min(tgt,x.spd+0.5):Math.max(tgt,x.spd-0.5);
 const grade=(cfg.g0+(cfg.gmax-cfg.g0)*inc/cfg.maxLvl)/100,run=x.spd>=7,mpm=x.spd*1000/60;
 const hrT=Math.min(h.max-2,(run?78+7.6*x.spd:60+9.8*x.spd)+150*grade*(x.spd/6)+(x.fit??0));
 x.hr+=(hrT-x.hr)*0.035+(rnd()-0.5)*0.8;
 const vo2=(run?0.2*mpm+0.9*mpm*grade:0.1*mpm+1.8*mpm*grade)+3.5;
 x.cal+=vo2/3.5*3.5*cfg.weight/200/60;x.dist+=x.spd/3600;x.asc=(x.asc??0)+Math.max(0,grade)*x.spd/3.6;
 const z=zoneOf(x.hr,h.b);x.zs[z]++;x.status=st;x.t++;
 const ph=st.startsWith('Warm-up')?'W':st.startsWith('Cool-down')?'C':/^High |^Fartlek .* Fast /.test(st)?'H':/^Recovery |^Fartlek .* Easy /.test(st)?'L':/^Z\d:/.test(st)?'R':'';
 if(ph&&ph!==x.ph){x.phases.push([x.t-1,ph]);x.ph=ph;}
 x.rows.push([x.t,x.t,Math.round(x.hr),+x.spd.toFixed(1),+inc.toFixed(1),+x.dist.toFixed(5),+x.cal.toFixed(2),0,z,PULSE.includes(cfg.program)?cfg.zone:0,grade*100]);}
function routeLevel(pts,km){let l=pts[0]?.[1]??0;for(const p of pts){if(p[0]>km)break;l=p[1];}return l;}
const newSession=(cfg,start)=>({cfg,start,plan:plan(cfg),t:0,spd:0,tgt:null,inc:0,hr:66+rnd()*4,dist:0,cal:0,zs:[0,0,0,0,0,0],rows:[],phases:[],ph:''});

// --- workout archive (what /workouts/* serves) ---
const W=new Map(),TRASH=new Set();let nextId=1;
const wid=n=>'w'+String(n).padStart(10,'0');
function finalize(x){const id=wid(nextId++),r=x.rows,h=hrModel(),hrs=r.map(q=>q[2]).filter(v=>v>0),mov=r.filter(q=>q[3]>0),dur=x.t,end=x.start+dur*1000,cfg=x.cfg;
 const blocks=[];for(let a=0;a<dur;a+=300){const b=r.filter(q=>q[1]>a&&q[1]<=a+300);if(!b.length)break;const av=k=>b.reduce((s,q)=>s+q[k],0)/b.length;blocks.push([Math.min(dur,a+300),+av(3).toFixed(2),+av(10).toFixed(2),Math.round(av(2))]);}
 const trimp=x.zs.reduce((s,v,i)=>s+v*i,0)/60,met=r.length?r.reduce((s,q,i)=>s+(i?q[6]-r[i-1][6]:q[6]),0)/(dur/60)/(3.5*cfg.weight/200):0;
 const summary={schema_version:1,timestamp_format:'iso8601_utc',id,start:new Date(x.start).toISOString().replace(/\.\d+Z$/,'Z'),end:new Date(end).toISOString().replace(/\.\d+Z$/,'Z'),duration_sec:dur,elapsed_sec:dur,samples:r.length,
  distance_km:+x.dist.toFixed(4),calories:+x.cal.toFixed(1),avg_speed_kmh:+(mov.reduce((s,q)=>s+q[3],0)/Math.max(1,mov.length)).toFixed(2),max_speed_kmh:Math.max(0,...r.map(q=>q[3])),
  avg_incline_pct:+(r.reduce((s,q)=>s+q[10],0)/Math.max(1,r.length)).toFixed(2),ascent_m:+(x.asc??0).toFixed(1),avg_heart_rate:hrs.length?+(hrs.reduce((a,b)=>a+b,0)/hrs.length).toFixed(1):null,max_heart_rate:hrs.length?Math.max(...hrs):null,
  wall_time_valid:true,interrupted:false,grade_estimated:true,distance_estimated:true,calories_estimated:true,calories_source:'firmware_total_calories',speed_source:'live_target',
  calibration_max_level:cfg.maxLvl,calibration_real_max_grade:cfg.gmax,calibration_zero_grade:cfg.g0,fat_g:+(x.cal*0.45/9).toFixed(1),carbs_g:+(x.cal*0.55/4).toFixed(1),avg_met:+met.toFixed(2),avg_vo2:+(met*3.5).toFixed(1),
  profile:{weight_kg:cfg.weight,age:num('number/Age'),sex:str('select/Gender')==='Female'?'female':'male',hr_max:h.max},zone_sec:x.zs,target_zone:PULSE.includes(cfg.program)?cfg.zone:null,
  phases:x.phases,trimp:+trimp.toFixed(1),blocks_sec:300,blocks,zwift_profile:null,route:x.routeName??null,program:cfg.program==='None'?'':cfg.program};
 W.set(id,{summary,rows:r,program:cfg.program});return id;}
function csv(id){const w=W.get(id),s0=Date.parse(w.summary.start);let o='timestamp,heart_rate,speed_kmh,incline_pct,distance_km,time_s,calories,status,program,zone,incline_level,target_zone,elapsed_s\n';
 for(const q of w.rows)o+=`${new Date(s0+q[1]*1000).toISOString().replace(/\.\d+Z$/,'Z')},${q[2]},${q[3].toFixed(3)},${q[10].toFixed(3)},${q[5].toFixed(6)},${q[1]},${q[6].toFixed(2)},running,"${w.program}",${q[8]},${q[4].toFixed(2)},${q[9]},${q[0]}\n`;return o;}

// A few made-up workouts, one of each kind, so History and Progress have something to show.
// Map profile: [km, incline level] every 100 m from the sample route's elevation (level 15 = 6.5 %, as the seed calibration).
function profileOf(text){try{const g=JSON.parse(text).geom,R=6371,rad=Math.PI/180,pts=[[0,0]];let km=0,k0=0,e0=g[0][2];
 for(let i=1;i<g.length;i++){const [a1,o1]=g[i-1],[a2,o2,e2]=g[i],d=R*Math.acos(Math.min(1,Math.sin(a1*rad)*Math.sin(a2*rad)+Math.cos(a1*rad)*Math.cos(a2*rad)*Math.cos((o2-o1)*rad)));km+=d;
  if(km-k0>=0.1){const gr=(e2-e0)/((km-k0)*10);pts.push([+km.toFixed(3),Math.max(0,Math.min(15,Math.round(gr*15/6.5)))]);k0=km;e0=e2;}}return {km,pts};}catch{return null;}}
(function seedHistory(){const day=864e5,now=Date.now(),rid=Object.keys(SAMPLE_ROUTES).find(k=>/Набережная/.test(SAMPLE_ROUTES[k]))??Object.keys(SAMPLE_ROUTES)[0],rp=rid?profileOf(SAMPLE_ROUTES[rid]):null;
 const runs=[[41,'Pulse Zone',{zone:2,run:28,warm:5,cool:3}],[34,'Pulse Zone',{zone:2,run:32,warm:5,cool:3}],[27,'Pulse Zone',{zone:2,run:30,warm:5,cool:3}],[20,'Pulse Zone',{zone:2,run:30,warm:5,cool:3}],[13,'None',{run:rp?Math.round(rp.km/8*60):25,warm:3,spd:8,route:rp}],[6,'HIIT',{hi:60,lo:90,cycles:8,high:10.5,low:5.5,hiitManual:true,warm:5,cool:3}],[2,'Fartlek',{fplan:'240/6.0,60/10.5,120/6.5,30/12.0,90/6.0,180/9.0,120/6.0,45/11.0,120/6.0',warm:5,cool:3}]];
 runs.forEach(([d,p,o],i)=>{const c={...cfgNow(),program:p,warm:0,cool:0,run:0,...o};const st=new Date(now-d*day);st.setHours(7+(i%4)*4,(i*17)%60,0,0);
  const x=newSession(c,st.getTime());x.fit=3-(i%4)*2;if(o.route){x.route=true;x.routeName=JSON.parse(SAMPLE_ROUTES[rid]).name;}const total=x.plan.reduce((s,g)=>s+(Number.isFinite(g.d)?g.d:0),0);
  for(let t=0;t<total;t++)step(x,c,{spd:o.spd??7,inc:0,route:o.route?o.route.pts:null});finalize(x);});})();

// --- live session ---
let S={st:'stopped',x:null},speed=1;
function pubIdle(){set('binary_sensor/Treadmill Usage',false);set('text_sensor/UI Control State','stopped|'+(str('select/Workout Program')==='None'?'manual':'auto'));}
function start(){if(S.st!=='stopped')return;const cfg=cfgNow();if(cfg.program==='None'&&num('number/Set Speed')<0.6)set('number/Set Speed',3);
 const x=newSession(cfg,Date.now());x.hr=S.hr??x.hr;const m=str('select/Map');if(m==='Custom route'&&ROUTE.points.length>1){x.route=true;x.routeName=ROUTE.name;}
 S={st:'running',x};set('binary_sensor/Treadmill Usage',true);set('text_sensor/UI Control State','running|'+(cfg.program==='None'?'manual':'auto'));set('text_sensor/Workout Recorder Status','recording');set('text_sensor/Training Status','Running');}
function stop(){if(S.st==='stopped')return;const x=S.x;S={st:'stopped',x:null,hr:x.hr};if(x.t>=num('number/Min Workout To Save')*60){const id=finalize(x),s=W.get(id).summary;
  for(const [k,v] of [['Last Time',s.duration_sec],['Last Distance',s.distance_km],['Last Calories',s.calories],['Last Avg Speed',s.avg_speed_kmh],['Last Avg HR',s.avg_heart_rate??0],['Last Max HR',s.max_heart_rate??0]])set('sensor/'+k,v);}
 for(const k of ['Treadmill Speed','Treadmill Speed Motor','Treadmill Speed Feedback'])set('sensor/'+k,0);set('text_sensor/Status',x.done?(x.cfg.program==='Fartlek'?'Fartlek: done':'Program: Done'):'');
 set('text_sensor/Workout Recorder Status','idle');set('text_sensor/Training Status','Idle');pubIdle();}
function tick(){const now=Date.now();if(S.st==='running'){const x=S.x,man={spd:num('number/Set Speed'),inc:num('number/Set Incline'),route:x.route?ROUTE.points:null};
  for(let i=0;i<speed&&S.st==='running';i++){step(x,x.cfg,man);if(x.done)break;}
  set('sensor/Treadmill Time',x.t);set('sensor/Treadmill Speed',x.spd);set('sensor/Treadmill Speed Motor',x.spd);set('sensor/Treadmill Speed Feedback',x.spd);set('sensor/Treadmill Incline',x.inc);set('sensor/Treadmill Incline Position',x.inc);
  set('sensor/Treadmill Distance',x.dist);set('sensor/Calories Burned',x.cal);set('sensor/Heart Rate',Math.round(x.hr));set('text_sensor/Status',x.status);
  x.zs.slice(1).forEach((v,i)=>set('sensor/Zone'+(i+1),v));const hrs=x.rows.map(q=>q[2]);set('sensor/Average Heart Rate',Math.round(hrs.reduce((a,b)=>a+b,0)/hrs.length));set('sensor/Max Heart Rate',Math.max(...hrs));
  if(x.done)stop();}
 else if(S.st==='paused'){S.x.hr+=(88-S.x.hr)*0.03+(rnd()-0.5)*0.6;set('sensor/Heart Rate',Math.round(S.x.hr));}
 else{S.hr=(S.hr??68)+(67-(S.hr??68))*0.05+(rnd()-0.5)*0.6;set('sensor/Heart Rate',Math.round(S.hr));}
 set('sensor/Uptime',(now-T0)/1000);set('sensor/Motor',27+(S.st==='running'?Math.min(18,S.x.t/120):0));}
const T0=Date.now();

// --- small stores the page uses ---
const LS={get:(k,d)=>{try{return JSON.parse(localStorage.getItem('tmdemo.'+k))??d;}catch{return d;}},set:(k,v)=>{try{localStorage.setItem('tmdemo.'+k,JSON.stringify(v));}catch{}}};
const PROG=new Map([['p1a2b3c4d5e6f7a8b',{version:1,id:'p1a2b3c4d5e6f7a8b',name:'Темповая 35 мин',goal:'темп',plan:'Неделя 1',schema_units:{duration:'s',speed:'km/h',incline:'level'},blocks:[{repeat:1,steps:[{duration:300,zone:1,incline:0}]},{repeat:3,steps:[{duration:360,speed:9.5,incline:1},{duration:120,speed:6,incline:0}]},{repeat:1,steps:[{duration:300,zone:1,incline:0}]}]}],
 ['p0f1e2d3c4b5a6978',{version:1,id:'p0f1e2d3c4b5a6978',name:'Длинная в Z2',goal:'выносливость',plan:'Неделя 1',schema_units:{duration:'s',speed:'km/h',incline:'level'},blocks:[{repeat:1,steps:[{duration:2700,zone:2,incline:1}]}]}]]);
const ROUTES=new Map(Object.entries({...SAMPLE_ROUTES,...LS.get('routes',{})}));   // sample routes (Ufa) + the visitor's own
let ROUTE={name:'',points:[]};
let jobN=0;const JOBS=new Map();

// --- HTTP: ESPHome REST + treadmill endpoints ---
const J=(o,status=200)=>new Response(JSON.stringify(o),{status,headers:{'Content-Type':'application/json'}}),T=(t,status=200,type='text/plain')=>new Response(t,{status,headers:{'Content-Type':type}});
async function handle(method,u,body){const p=decodeURIComponent(u.pathname),q=u.searchParams;
 if(p==='/workouts/status')return J({state:S.st==='stopped'?'idle':'recording',recording:S.st!=='stopped',archive_available:true,error:'',total_bytes:7340032,used_bytes:180000+W.size*42000});
 if(p==='/workouts/list'||p==='/api/workouts/trash/list'){const tr=p!=='/workouts/list',ids=[...W.keys()].filter(i=>TRASH.has(i)===tr).sort(),after=q.get('after')||'',lim=Math.min(20,+q.get('limit')||20),rest=ids.filter(i=>i>after),page=rest.slice(0,lim);
  return J({items:page.map(id=>({id,summary_available:true,...(q.get('summaries')?{summary:W.get(id).summary}:{})})),next:rest.length>lim?page.at(-1):null});}
 if(p==='/workouts/file'){const id=q.get('id');if(!W.has(id)||TRASH.has(id))return J({error:'not_found'},404);return q.get('type')==='csv'?T(csv(id),200,'text/csv'):J(W.get(id).summary);}
 if(/^\/api\/workouts\/(trash|restore|purge)$/.test(p)&&method==='POST'){const op=p.split('/').pop(),id=q.get('id');if(!W.has(id))return J({error:'invalid_id'},404);if(op==='trash')TRASH.add(id);if(op==='restore')TRASH.delete(id);if(op==='purge'){W.delete(id);TRASH.delete(id);}
  const job=++jobN;JOBS.set(job,{job,state:'succeeded',operation:op,id,error:''});return J({job,state:'queued',operation:op,id});}
 if(p==='/api/workouts/job'){const j=JOBS.get(+q.get('job'));return j?J(j):J({error:'unknown_job'},404);}
 if(p==='/api/workouts/live'){const x=S.x,from=+q.get('from')||0,rows=x?x.rows.slice(from,from+300).map(r=>[r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9]]):[];
  return J({id:x?wid(nextId):null,active:!!x,program:x?x.cfg.program:'',start_epoch:x?Math.floor(x.start/1000):0,total:x?x.rows.length:0,from,columns:['elapsed_s','time_s','heart_rate','speed_kmh','incline_level','distance_km','calories','paused','zone','target_zone'],rows});}
 if(p==='/api/storage/scan')return J({state:'ready',used_bytes:180000+W.size*42000,total_bytes:7340032,file_count:W.size*2,file_bytes:W.size*42000});
 if(p==='/api/presets'){const k=q.get('kind');if(method==='POST'){try{LS.set('presets.'+k,JSON.parse(body).items);}catch{return J({error:'bad_json'},400);}return J({saved:true});}return J({items:LS.get('presets.'+k,[])});}
 if(p==='/api/programs/list')return J({items:[...PROG.keys()].map(id=>({id}))});
 if(p==='/api/programs/file')return PROG.has(q.get('id'))?J(PROG.get(q.get('id'))):J({error:'program_not_found'},404);
 if(p==='/api/programs/delete'){PROG.delete(q.get('id'));set('sensor/Programs Revision',num('sensor/Programs Revision')+1);return J({deleted:true});}
 if(p==='/api/programs/save'){try{const o=JSON.parse(body);PROG.set(o.id,o);}catch{return J({error:'bad_json'},400);}set('sensor/Programs Revision',num('sensor/Programs Revision')+1);return J({saved:true});}
 if(p==='/api/routes/list')return J({items:[...ROUTES.entries()].map(([id,t])=>({id,size:t.length}))});
 if(p==='/api/routes/file')return ROUTES.has(q.get('id'))?T(ROUTES.get(q.get('id')),200,'application/json'):J({error:'not_found'},404);
 if(p==='/api/routes/save'){ROUTES.set(q.get('id'),body);LS.set('routes',Object.fromEntries(ROUTES));return J({saved:true});}
 if(p==='/api/routes/delete'){ROUTES.delete(q.get('id'));LS.set('routes',Object.fromEntries(ROUTES));return J({deleted:true});}
 if(p==='/api/routes/act')return J({saved:true});
 if(p==='/api/route/active'){if(method==='POST'){const l=String(body).trim().split('\n');ROUTE={name:l[1]||'',points:l.slice(2).map(s=>s.split(' ').map(Number)).filter(a=>a.length===2&&a.every(Number.isFinite))};return J({saved:true});}return J({name:ROUTE.name,option:'Custom route',points:ROUTE.points});}
 if(p==='/mcp')return J({jsonrpc:'2.0',id:null,error:{code:-32601,message:'MCP is not available in the demo'}});
 // ESPHome REST: /<domain>/<name>[/<action>]
 const m=p.match(/^\/(sensor|binary_sensor|text_sensor|number|select|switch|button|text|update)\/([^/]+)(?:\/([a-z_]+))?$/);
 if(m){const id=m[1]+'/'+m[2],e=ent(id);if(!e)return J({error:'not_found'},404);if(method!=='POST'||!m[3])return J(e);act(e,m[3],q);return T('',200);}
 return J({error:'not_found'},404);}
function act(e,a,q){const id=e.id;
 if(e.domain==='number'&&a==='set'){let v=+q.get('value');const lo=+e.min_value,hi=+e.max_value,s=+e.step||1;if(!Number.isFinite(v))return;v=Math.min(hi,Math.max(lo,Math.round((v-lo)/s)*s+lo));set(id,+v.toFixed(DEC.get(id)));}
 else if(e.domain==='select'&&a==='set'){const o=q.get('option');if((e.option||[]).includes(o))set(id,o);if(id==='select/Workout Program'&&S.st==='stopped')pubIdle();}
 else if(e.domain==='text'&&a==='set')set(id,q.get('value')??'');
 else if(e.domain==='switch')set(id,a==='turn_on'||(a==='toggle'&&!e.value));
 else if(e.domain==='button'&&a==='press'){const n=e.name;
  if(n==='Start Treadmill')start();else if(n==='Stop Treadmill')stop();
  else if(n==='Pause Training'&&S.st==='running'){S.st='paused';set('text_sensor/UI Control State','paused|'+(S.x.cfg.program==='None'?'manual':'auto'));for(const k of ['Treadmill Speed','Treadmill Speed Motor'])set('sensor/'+k,0);}
  else if(n==='Resume Training'&&S.st==='paused'){S.st='running';set('text_sensor/UI Control State','running|'+(S.x.cfg.program==='None'?'manual':'auto'));}
  else if(n==='HR Scan'){set('text_sensor/HR Scan Result','scanning');setTimeout(()=>set('text_sensor/HR Scan Result','A0:9E:1A:11:22:33|Polar H10 1A2B3C4D|-58;D4:36:39:44:55:66|HRM-Dual:123456|-71'),2500);}}
 if(['number/Age','select/Gender','select/Heart Rate Zone','select/HR Max Mode','number/HR Max Manual','select/HR Zone Method','number/Resting Heart Rate'].includes(id))pubZones();}

// --- install: fetch + EventSource ---
const realFetch=window.fetch.bind(window);
window.fetch=async(input,init={})=>{const u=new URL(typeof input==='string'?input:input.url,location.href);
 if(u.origin!==location.origin||/\.(html|js|css|json|png|svg|ico)$/.test(u.pathname)&&!u.pathname.startsWith('/api/'))return realFetch(input,init);
 const method=(init.method||'GET').toUpperCase();await new Promise(r=>setTimeout(r,40+rnd()*60));return handle(method,u,init.body);};
class DemoEventSource{constructor(){this.readyState=0;this.l={};listeners.add(this.f=(t,d)=>this.fire(t,{data:d}));
 setTimeout(()=>{this.readyState=1;this.fire('open',{});for(const e of E.values())this.fire('state',{data:JSON.stringify(e)});},120);this.ping=setInterval(()=>this.fire('ping',{data:''}),5000);}
 addEventListener(t,f){(this.l[t]??=[]).push(f);}removeEventListener(t,f){this.l[t]=(this.l[t]||[]).filter(x=>x!==f);}
 fire(t,ev){if(this.readyState===2)return;for(const f of this.l[t]||[])f(ev);this['on'+t]?.(ev);}
 close(){this.readyState=2;listeners.delete(this.f);clearInterval(this.ping);}}
DemoEventSource.CONNECTING=0;DemoEventSource.OPEN=1;DemoEventSource.CLOSED=2;window.EventSource=DemoEventSource;

// --- boot ---
{const u=ent('update/Firmware Update');if(u)Object.assign(u,{state:'NO UPDATE',value:VERSION,current_version:VERSION,title:'Treadmill'});}
set('text_sensor/Pulse monitor','Подключен');pubZones();pubIdle();setInterval(tick,1000);
// banner: what this is + simulation speed
addEventListener('DOMContentLoaded',()=>{const b=document.createElement('div');b.id='demo-banner';let ru=document.documentElement.lang!=='en';
 b.style.cssText='position:fixed;left:12px;bottom:12px;z-index:99999;display:flex;gap:8px;align-items:center;padding:8px 12px;border-radius:12px;background:rgba(20,24,32,.92);color:#cfd6e4;font:13px system-ui;border:1px solid #2c3442;box-shadow:0 4px 20px rgba(0,0,0,.35)';
 const sp=document.createElement('button');sp.style.cssText='font:inherit;color:#fff;background:#2b6cb0;border:0;border-radius:8px;padding:3px 9px;cursor:pointer';
 const lab=()=>{sp.textContent=(ru?'Скорость ':'Speed ')+speed+'×';sp.title=ru?'Ускорить симуляцию':'Speed up the simulation';};lab();sp.onclick=()=>{speed=speed===1?10:speed===10?30:1;lab();};
 const t=document.createElement('span');
 const txt=()=>{t.innerHTML=(ru?'<b>Демо</b> · виртуальная дорожка, данные выдуманы':'<b>Demo</b> · virtual treadmill, made-up data')+' · <a href="https://github.com/samsonovss/ESPHome-Treadmill-FTMS" style="color:#7fb3ff" target="_blank" rel="noopener">GitHub</a>';lab();};
 b.append(t,sp);document.body.append(b);txt();setInterval(()=>{const r=document.documentElement.lang!=='en';if(r!==ru){ru=r;txt();}},1000);});
})();
