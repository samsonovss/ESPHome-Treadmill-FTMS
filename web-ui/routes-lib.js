'use strict';
// Route library: user routes live as opaque JSON files on the treadmill's LittleFS
// (/api/routes/*); the belt only ever gets the incline profile of ONE route
// (/api/route/active → map option "Custom route"). Geometry is [lat, lon, ele] (m).
const RoutesLib=(()=>{
 const CUSTOM='Custom route',LS='tm.routes.v1',FAV='tm.routes.fav';
 let cal=(()=>{try{const c=JSON.parse(localStorage.getItem('tm.routes.cal'));if(c&&c.gmax-c.g0>=0.1&&c.max>=1)return c;}catch{}return {g0:0,gmax:15,max:15,mstep:0.3};})() /* last known treadmill calibration — no redraw with defaults before the entities arrive (13.10) */,items=new Map(),loaded=false,loading=null,active=null,activeAt=0,listeners=new Set();
 try{for(const r of JSON.parse(localStorage.getItem(LS)||'[]'))if(r&&r.id&&Array.isArray(r.geom))items.set(r.id,r);}catch{}
 const notify=()=>{for(const f of listeners)try{f();}catch{}};
 const persist=()=>{try{localStorage.setItem(LS,JSON.stringify([...items.values()]));}catch{}};
 const favs=()=>{try{return new Set(JSON.parse(localStorage.getItem(FAV)||'[]'));}catch{return new Set();}};
 const rad=d=>d*Math.PI/180;
 function hav(a,b){const dl=rad(b[0]-a[0]),dn=rad(b[1]-a[1]),h=Math.sin(dl/2)**2+Math.cos(rad(a[0]))*Math.cos(rad(b[0]))*Math.sin(dn/2)**2;return 12742*Math.asin(Math.min(1,Math.sqrt(h)));}
 function cumulative(g){const d=[0];for(let i=1;i<g.length;i++)d.push(d[i-1]+hav(g[i-1],g[i]));return d;}
 // Detail: segment length 50 m (smoothing ±60 m) or 25 m (±30 m). Elevation sampled every 5 m.
 // All routes are smooth (10.10): grade every 10 m, level in tenths, sent once it moves by the motor step (like Zwift live).
 // `detail` now only picks the elevation smoothing by track quality: 25 (and 10) = light ±20 m, 40 = medium ±40 m, 50 = strong ±60 m.
 const SAMPLE=0.005,DETAIL={50:{step:0.01,half:12},40:{step:0.01,half:8},25:{step:0.01,half:4},10:{step:0.01,half:4}},det=r=>DETAIL[r?.detail]??DETAIL[50];
 // Smoothing removes GPS/DEM noise that would otherwise jump levels.
 function elevations(g,half){const d=cumulative(g),end=d.at(-1),n=Math.max(2,Math.floor(end/SAMPLE)+1),out=[];let j=0;
  for(let i=0;i<n;i++){const k=Math.min(end,i*SAMPLE);while(j<d.length-2&&d[j+1]<k)j++;const t=d[j+1]>d[j]?(k-d[j])/(d[j+1]-d[j]):0;out.push(g[j][2]+(g[j+1][2]-g[j][2])*Math.max(0,Math.min(1,t)));}
  const pre=[0];for(const v of out)pre.push(pre.at(-1)+v);const sm=out.map((_,i)=>{const a=Math.max(0,i-half),b=Math.min(out.length-1,i+half);return (pre[b+1]-pre[a])/(b-a+1);});return {end,ele:sm};}
 // Calibration from firmware: real grade g0 % at level 0 and gmax % at the max level (linear between).
 // Level = (grade × intensity − g0) / (gmax − g0) × max; below g0 (incl. descents) → 0.
 function setCal(c){const g0=Number.isFinite(c?.g0)?c.g0:0,gmax=Number.isFinite(c?.gmax)?c.gmax:15,max=Number.isFinite(c?.max)&&c.max>=1?Math.min(15,c.max):15;const mstep=Number.isFinite(c?.mstep)&&c.mstep>=0.1?Math.min(1,c.mstep):0.3;cal=gmax-g0>=0.1?{g0,gmax,max,mstep}:{g0:0,gmax:15,max:15,mstep};if(c?.real){const k=JSON.stringify(cal);if(k!==setCal.k){setCal.k=k;try{localStorage.setItem('tm.routes.cal',k);}catch{}}}}
 const toLevel=g=>Math.max(0,Math.min(cal.max,Math.round((g-cal.g0)/(cal.gmax-cal.g0)*cal.max))),levelPct=l=>cal.g0+l/cal.max*(cal.gmax-cal.g0);
 const toLevel10=g=>Math.max(0,Math.min(cal.max,Math.round((g-cal.g0)/(cal.gmax-cal.g0)*cal.max*10)/10));
 function profile(r){const s=series(r);if(!s)return null;const k=r.intensity??1,smooth=true;let pts=[],max=0;
  if(smooth){// new value only when it differs from the last one sent by ≥ motor step (smaller moves the motor ignores); 0 always reachable
   const build=th=>{const o=[];let mx=0;for(let i=0;i+1<s.km.length;i++){const lv=toLevel10(s.grade[i]*k);if(!o.length||Math.abs(lv-o.at(-1)[1])>=th-1e-6||(lv===0&&o.at(-1)[1]!==0)){o.push([+s.km[i].toFixed(3),lv]);mx=Math.max(mx,lv);}}return [o,mx];};
   let th=cal.mstep;[pts,max]=build(th);while(pts.length>1000&&th<3){th+=0.1;[pts,max]=build(th);}}
  else for(let i=0;i+1<s.km.length;i++){const lv=toLevel(s.grade[i]*k);max=Math.max(max,lv);if(!pts.length||pts.at(-1)[1]!==lv)pts.push([+s.km[i].toFixed(3),lv]);}
  const endKm=+s.end.toFixed(3);if(pts.at(-1)[0]<endKm)pts.push([endKm,pts.at(-1)[1]]);
  return {points:pts,endKm,ascent:s.ascent,max};}
 // Per-segment series (smoothed elevation, raw grade before intensity) + km index of geometry vertices; cached per geom+detail.
 const cache=new WeakMap();
 function series(r){const g=r?.geom;if(!g||g.length<2)return null;const D=det(r),ck=D.step+'/'+D.half;let m=cache.get(g);if(!m)cache.set(g,m={});if(m[ck])return m[ck];
  const {end,ele}=elevations(g,D.half),per=Math.round(D.step/SAMPLE),km=[],el=[],gr=[];let ascent=0,descent=0;
  for(let i=0;i+1<ele.length;i++){const dz=ele[i+1]-ele[i];if(dz>0)ascent+=dz;else descent-=dz;}
  for(let a=0;a<ele.length-1;a+=per){const b=Math.min(ele.length-1,a+per);km.push(a*SAMPLE);el.push(ele[a]);gr.push((ele[b]-ele[a])/((b-a)*SAMPLE*1000)*100);}
  if(!km.length){km.push(0);el.push(ele[0]);gr.push(0);}km.push(end);el.push(ele.at(-1));gr.push(gr.at(-1));
  return m[ck]={end,step:D.step,km,ele:el,grade:gr,ascent:Math.round(ascent),descent:Math.round(descent),cum:cumulative(g)};}
 // Point on the route at distance km (clamped) → [lat, lon]; nearest vertex → km.
 function pointAt(r,k){const s=series(r);if(!s)return null;const g=r.geom,d=s.cum;k=Math.max(0,Math.min(d.at(-1),k));let lo=0,hi=d.length-1;while(hi-lo>1){const m=(lo+hi)>>1;d[m]<=k?lo=m:hi=m;}
  const t=d[hi]>d[lo]?(k-d[lo])/(d[hi]-d[lo]):0;return [g[lo][0]+(g[hi][0]-g[lo][0])*t,g[lo][1]+(g[hi][1]-g[lo][1])*t];}
 function kmNear(r,lat,lon){const s=series(r);if(!s)return null;const g=r.geom,cl=Math.cos(rad(lat));let best=0,bd=Infinity;for(let i=0;i<g.length;i++){const a=g[i][0]-lat,b=(g[i][1]-lon)*cl,dd=a*a+b*b;if(dd<bd){bd=dd;best=i;}}return s.cum[best];}
 // Geometry between km a and b (interpolated ends) → [[lat,lon],...].
 function slice(r,a,b){const s=series(r);if(!s)return [];const g=r.geom,d=s.cum,out=[pointAt(r,a)];for(let i=0;i<g.length;i++)if(d[i]>a&&d[i]<b)out.push([g[i][0],g[i][1]]);out.push(pointAt(r,b));return out;}
 // Climbs from the level profile: runs with level ≥ 1 (gaps < 100 m merged), ≥ 150 m long; avg = road grade from elevation.
 function climbs(r){const s=series(r),p=r.points;if(!s||!p?.length)return [];const runs=[];
  for(let i=0;i<p.length-1;i++){const a=p[i][0],b=p[i+1][0];if(p[i][1]>=1){const last=runs.at(-1);if(last&&a-last[1]<0.1)last[1]=b;else runs.push([a,b]);}}
  const eleAt=k=>{const i=Math.min(s.km.length-1,Math.max(0,Math.round(k/s.step)));return s.ele[i];};
  return runs.filter(([a,b])=>b-a>=0.15).map(([a,b])=>({a,b,gain:eleAt(b)-eleAt(a),avg:(eleAt(b)-eleAt(a))/((b-a)*1000)*100})).filter(c=>c.avg>=1&&c.gain>=3);}
 // Difficulty on THIS treadmill: distance + climbing the belt will actually do (grade above level 0, after calibration);
 // 100 m of climbing ≈ 2 km of flat running (trail-style weighting). Bands of the flat-equivalent distance.
 const DIFF=[['Лёгкая','Easy'],['Средняя','Moderate'],['Сложная','Hard'],['Очень сложная','Very hard']];
 function difficulty(r){const p=r?.points,end=r?.endKm;if(!p?.length||!(end>0))return null;let up=0;
  for(let i=0;i<p.length;i++){const a=p[i][0],b=i+1<p.length?p[i+1][0]:end;up+=Math.max(0,levelPct(p[i][1])-cal.g0)/100*Math.max(0,b-a)*1000;}
  const eq=end+up/50,level=eq<5?0:eq<9?1:eq<14?2:3;return {eq:+eq.toFixed(1),up:Math.round(up),level,label:DIFF[level][0],en:DIFF[level][1]};}
 // Value of a [[km,v],...] series at km (step for levels, linear otherwise).
 function at(km,arr,k){let i=0;while(i+1<km.length&&km[i+1]<=k)i++;return arr[i];}
 function simplify(g,maxPts=1500){if(g.length<=maxPts)return g;let tol=0.002;// km
  const run=t=>{const keep=new Uint8Array(g.length);keep[0]=keep[g.length-1]=1;const st=[[0,g.length-1]];
   while(st.length){const [a,b]=st.pop();let best=-1,bd=0;const ax=g[a][1]*Math.cos(rad(g[a][0]))*111.32,ay=g[a][0]*110.57,bx=g[b][1]*Math.cos(rad(g[b][0]))*111.32,by=g[b][0]*110.57,L=Math.hypot(bx-ax,by-ay)||1e-9;
    for(let i=a+1;i<b;i++){const px=g[i][1]*Math.cos(rad(g[i][0]))*111.32,py=g[i][0]*110.57,dd=Math.abs((bx-ax)*(ay-py)-(ax-px)*(by-ay))/L+Math.abs(g[i][2]-(g[a][2]+(g[b][2]-g[a][2])*(i-a)/(b-a)))/1000*4;if(dd>bd){bd=dd;best=i;}}
    if(best>0&&bd>t){keep[best]=1;st.push([a,best],[best,b]);}}
   return g.filter((_,i)=>keep[i]);};
  let out=run(tol);while(out.length>maxPts){tol*=1.6;out=run(tol);}return out;}
 const r6=v=>Math.round(v*1e6)/1e6,r1=v=>Math.round(v*10)/10;
 function normalize(g){return simplify(g.filter(p=>p.every(Number.isFinite))).map(p=>[r6(p[0]),r6(p[1]),r1(p[2])]);}
 async function fillElevation(g){// Open-Meteo DEM, 100 points per request (only for GPX without <ele>)
  const out=g.map(p=>[...p]);for(let i=0;i<out.length;i+=100){const s=out.slice(i,i+100),u='https://api.open-meteo.com/v1/elevation?latitude='+s.map(p=>p[0].toFixed(5)).join(',')+'&longitude='+s.map(p=>p[1].toFixed(5)).join(',');
   const j=await (await fetch(u)).json();if(!Array.isArray(j.elevation)||j.elevation.length!==s.length)throw Error('Нет высот: GPX без <ele>, а сервис высот недоступен');s.forEach((p,q)=>p[2]=j.elevation[q]);}return out;}
 // Track quality from the raw GPX: point spacing (median, moving points only), elevation source and sensor noise.
 // Noise = jitter estimated from second differences of elevation resampled every 5 m (σ = std(Δ²)/√6):
 // real terrain barely curves over 5 m, so hills do not count as noise; standing still does not shrink spacing.
 function quality(g,src){const d=cumulative(g),seg=[];for(let i=1;i<g.length;i++){const m=(d[i]-d[i-1])*1000;if(m>0.3)seg.push(m);}seg.sort((a,b)=>a-b);const spacing=seg.length?seg[seg.length>>1]:0;
  let noise=null;if(src==='file'){const e=[],end=d.at(-1);let j=0;for(let k=0;k<=end;k+=0.005){while(j<d.length-2&&d[j+1]<k)j++;const t=d[j+1]>d[j]?(k-d[j])/(d[j+1]-d[j]):0;e.push(g[j][2]+(g[j+1][2]-g[j][2])*Math.max(0,Math.min(1,t)));}
   if(e.length>4){let ss=0,n=0;for(let i=1;i+1<e.length;i++){const v=e[i+1]-2*e[i]+e[i-1];ss+=v*v;n++;}noise=Math.sqrt(ss/n/6);}else noise=0;}
  const rating=src!=='file'?'rough':spacing<=10&&noise<=0.3?'excellent':spacing<=60&&noise<=1?'good':'rough';
  return {src,spacing:+spacing.toFixed(1),noise:noise==null?null:+noise.toFixed(2),rating,detail:rating==='excellent'?25:rating==='good'?40:50};}
 async function parseGPX(text){const doc=new DOMParser().parseFromString(text,'application/xml');if(doc.querySelector('parsererror'))throw Error('Не похоже на GPX');
  let nodes=[...doc.getElementsByTagName('trkpt')];if(nodes.length<2)nodes=[...doc.getElementsByTagName('rtept')];if(nodes.length<2)throw Error('В GPX нет трека');
  let g=nodes.map(n=>[+n.getAttribute('lat'),+n.getAttribute('lon'),(e=>e?+e.textContent:NaN)(n.getElementsByTagName('ele')[0])]).filter(p=>Number.isFinite(p[0])&&Number.isFinite(p[1]));
  let q;if(g.filter(p=>Number.isFinite(p[2])).length<g.length*0.9){q=quality(g.map(p=>[p[0],p[1],0]),'dem');g=normalize(g.map(p=>[p[0],p[1],0]));g=await fillElevation(g);}else{let last=g.find(p=>Number.isFinite(p[2]))[2];g=g.map(p=>{if(Number.isFinite(p[2]))last=p[2];return [p[0],p[1],last];});q=quality(g,'file');}
  const name=(doc.querySelector('trk > name, rte > name, metadata > name')?.textContent||'').trim();return {name,geom:normalize(g),quality:q};}
 // BRouter (public, CORS): waypoints [[lat,lon],...] → road/trail geometry with elevation.
 async function brouter(wps,profile='trekking'){const u='https://brouter.de/brouter?lonlats='+wps.map(p=>p[1].toFixed(6)+','+p[0].toFixed(6)).join('|')+'&profile='+profile+'&alternativeidx=0&format=geojson';
  const r=await fetch(u);if(!r.ok)throw Error('Прокладка маршрута не удалась ('+r.status+')');const j=await r.json(),c=j.features?.[0]?.geometry?.coordinates;if(!Array.isArray(c)||c.length<2)throw Error('Маршрут не найден');
  return c.map(p=>[p[1],p[0],Number.isFinite(p[2])?p[2]:0]);}
 const newId=()=>'r'+[...crypto.getRandomValues(new Uint8Array(8))].map(b=>b.toString(16).padStart(2,'0')).join('');
 // Board Wi-Fi is slow while BLE shares the radio (~2–8 KB/s on 30–40 KB files) → generous timeout.
 async function req(path,opt={}){const {timeout=45000,...o}=opt,c=new AbortController(),t=setTimeout(()=>c.abort(),timeout);try{const r=await fetch(path,{cache:'no-store',...o,signal:c.signal});let j=null;try{j=await r.json();}catch{}if(!r.ok)throw Error(j?.error==='workout_active_or_finalizing'||j?.error==='fresh_stop_required'?'Дорожка должна стоять':j?.error||('HTTP '+r.status));return j;}finally{clearTimeout(t);}}
 async function load(force=false){if(loading)return loading;if(loaded&&!force)return;loading=(async()=>{try{const l=await req('/api/routes/list'),ids=new Set();
   for(const it of l.items||[]){ids.add(it.id);const have=items.get(it.id);if(have&&have._size===it.size)continue;try{const r=await req('/api/routes/file?id='+it.id);if(r&&r.id===it.id&&Array.isArray(r.geom)){r._size=it.size;items.set(it.id,r);}}catch{}}
   for(const id of [...items.keys()])if(!ids.has(id))items.delete(id);loaded=true;persist();for(const r of items.values())await syncAct(r);}catch{loaded=true;}finally{loading=null;notify();}})();return loading;}
 const placeTried=new Set();
 async function geocode(g,lang='ru'){try{const c=new AbortController(),tm=setTimeout(()=>c.abort(),4000);const j=await (await fetch(`https://nominatim.openstreetmap.org/reverse?format=jsonv2&zoom=10&accept-language=${lang==='en'?'en':'ru,en'}&lat=${g[0][0]}&lon=${g[0][1]}`,{signal:c.signal})).json();clearTimeout(tm);const a=j.address||{},city=a.city||a.town||a.village||a.municipality||a.county||a.state||'';return [city,a.country||''].filter(Boolean).join(', ').slice(0,60);}catch{return '';}}
 // Fills «place» (city, country) for saved routes that lack it; one request per route per page load, gently paced.
 let placeBusy=false;
 const sleep=ms=>new Promise(k=>setTimeout(k,ms)),lang=()=>document.querySelector('.concept-host')?.dataset.lang==='en'?'en':'ru';
 async function fillPlaces(){if(placeBusy)return;placeBusy=true;try{for(const r of [...items.values()]){if((r.place&&r.placeEn)||/^zwift/.test(r.source||'')||!r.geom?.length||placeTried.has(r.id))continue;placeTried.add(r.id);let pl=r.place||'',en=r.placeEn||'';if(!pl){pl=await geocode(r.geom,'ru');await sleep(1100);}if(pl&&!en)en=await geocode(r.geom,'en');if(pl&&(en||!r.place))await save({...r,place:pl,placeEn:en});await sleep(1100);}}finally{placeBusy=false;}}
 async function save(r){if(r.place==null&&!/^zwift/.test(r.source||'')&&r.geom?.length&&!(r.id&&placeTried.has(r.id))){const ru=await geocode(r.geom,'ru');await sleep(1100);r={...r,place:ru,placeEn:ru?await geocode(r.geom,'en'):''};}const body={v:1,place:String(r.place||'').slice(0,60),placeEn:String(r.placeEn||'').slice(0,60),id:r.id||newId(),name:String(r.name||'Маршрут').slice(0,80),created:r.created||Date.now(),intensity:r.intensity??1,source:r.source||'',detail:[10,25,40].includes(r.detail)?r.detail:50,quality:r.quality??null,geom:r.geom};
  const text=JSON.stringify(body);if(text.length>65000)throw Error('Маршрут слишком большой');await req('/api/routes/save?id='+body.id,{method:'POST',headers:{'Content-Type':'application/json'},body:text});body._size=text.length;items.set(body.id,body);persist();notify();syncAct(body);return body;}
 async function remove(id){await req('/api/routes/delete?id='+id,{method:'POST'});items.delete(id);persist();notify();}
 function actBody(r){const p=profile(r);if(!p)throw Error('Пустой маршрут');if(p.points.length>1024)throw Error('Слишком много точек профиля');
  const name=String(r.name).replace(/[\x00-\x1f\x7f]/g,' ').slice(0,60)||'Маршрут';
  return {name,p,text:'TREADMILL-ROUTE 1\n'+name+'\n'+p.points.map(([k,v])=>k.toFixed(3)+' '+(+v.toFixed(1))).join('\n')+'\n'};}
 async function upload(r){const {name,p,text}=actBody(r);
  await req('/api/route/active',{method:'POST',headers:{'Content-Type':'text/plain'},body:text});
  active={name,points:p.points};activeAt=Date.now();}
 // Display card (<id>.act on the board): ready-made profile so the Nextion can load the route without the web.
 async function syncAct(r){if(!r?.id||!r.geom?.length)return;const k='tm.act.'+r.id,sig=(r._size||0)+'|'+r.name+'|'+JSON.stringify(cal);
  try{if(localStorage.getItem(k)===sig)return;}catch{}
  const body=actBody(r).text;for(let t=0;t<3;t++){try{await req('/api/routes/act?id='+r.id,{method:'POST',headers:{'Content-Type':'text/plain'},body});try{localStorage.setItem(k,sig);}catch{}return;}catch(e){if(t===2)console.warn('act sync',r.id,e.message||e);else await sleep(1500);}}}
 async function refreshActive(){if(Date.now()-activeAt<5000)return active;activeAt=Date.now();try{const j=await req('/api/route/active');active=j&&Array.isArray(j.points)&&j.points.length>1?{name:j.name,points:j.points}:null;}catch{}notify();return active;}
 // Display name → route; names unique against the built-ins.
 function byName(builtin={}){const out={};for(const r of [...items.values()].sort((a,b)=>(a.created||0)-(b.created||0))){const p=profile(r);if(!p)continue;let n=r.name;while(builtin[n]||out[n])n+=' ·';const zr=r.source==='zwift-rec';out[n]={points:p.points,endKm:p.endKm,custom:r.id,ascent:p.ascent,geom:zr?null:r.geom,zwrec:zr,zwsaved:r.source==='zwift',detail:r.detail,intensity:r.intensity,quality:r.quality??null,place:(lang()==='en'&&r.placeEn)||r.place||''};}return out;}
 // Zwift catalog (09.10): index + per-route files on GitHub (route_profiles/zwift/). Index cached in this browser;
 // cards/levels come from the index coarse elevation (every 50 m), the real map geometry loads when a route is picked.
 const ZW_URL='https://raw.githubusercontent.com/samsonovss/ESPHome-Treadmill-FTMS/main/route_profiles/zwift/',ZWK='tm.zwcat.v2';
 const zwUrl=()=>{try{return localStorage.getItem('tm.zwcat.url')||ZW_URL;}catch{return ZW_URL;}};
 let zw=null,zwFailAt=0,zwState='',zwLoading=null;
 // 18.10: the catalog saved last time is used at once (no wait on «Zwift»); it is re-fetched only when older than a day
 try{const c=JSON.parse(localStorage.getItem(ZWK)||'null');if(c&&Array.isArray(c.routes))zw=c;}catch{}const zwGeoms=new Map(),zwGeomLoading=new Map(),zwSynth=new Map();
 try{const c=JSON.parse(localStorage.getItem(ZWK)||'null');if(c?.url===zwUrl()&&Array.isArray(c.routes))zw=c;}catch{}
 async function zwLoad(force=false){if(zwLoading)return zwLoading;if(!force&&(zw&&Date.now()-(zw.at||0)<864e5||Date.now()-zwFailAt<6e5))return;zwLoading=(async()=>{
   try{const u=zwUrl(),r=await fetch(u+'index.json',{cache:'no-cache'});if(!r.ok)throw Error('HTTP '+r.status);const j=await r.json();if(!Array.isArray(j.routes))throw Error('нет списка');
    zw={url:u,at:Date.now(),generated:j.generated,estep:j.estep||100,routes:j.routes};zwSynth.clear();try{localStorage.setItem(ZWK,JSON.stringify(zw));}catch{}zwState='';}
   catch(e){zwFailAt=Date.now();zwState=zw?'':'Каталог Zwift недоступен: '+e.message;}finally{zwLoading=null;notify();}})();return zwLoading;}
 async function zwGeom(slug){if(zwGeoms.has(slug))return zwGeoms.get(slug);if(zwGeomLoading.has(slug))return zwGeomLoading.get(slug);
  const p=(async()=>{try{const r=await fetch(zwUrl()+'routes/'+encodeURIComponent(slug)+'.json');if(!r.ok)throw Error('HTTP '+r.status);const j=await r.json();if(!Array.isArray(j.pts)||j.pts.length<2)throw Error('пустой маршрут');zwGeoms.set(slug,j.pts);notify();return j.pts;}finally{zwGeomLoading.delete(slug);}})();
  zwGeomLoading.set(slug,p);return p;}
 // Straight synthetic line (lat 0, 50 m steps) carrying the coarse elevation → same level math as real routes.
 const synth=r=>{let g=zwSynth.get(r.slug);if(!g){const d=(zw?.estep||100)/1000/111.195;g=r.e.map((e,i)=>[0,i*d,e]);zwSynth.set(r.slug,g);}return g;};
 let zwCache={k:'',out:{}};
 function zwByName(taken={}){if(!zw)return {};const k=JSON.stringify(cal)+'|'+zw.at+'|'+zwGeoms.size+'|'+Object.keys(taken).join('\u0001');if(zwCache.k===k)return zwCache.out;const out={};
  const flat=r=>Math.max(...r.e)-Math.min(...r.e)<=1.5;
  for(const r of zw.routes){if(!Array.isArray(r.e)||r.e.length<2)continue;
   if(flat(r))r.noele=true;
   // Levels always from the index elevation (same numbers before/after the map file loads → card never redraws);
   // the real geometry is only the map line.
   const real=zwGeoms.get(r.slug)??null,p=profile({geom:synth(r),detail:10}),ps=real?profile({geom:real,detail:10}):null;if(!p)continue;let n=r.name;while(taken[n]||out[n])n+=' ·';
   out[n]={points:ps?.points??p.points,cardPoints:p.points,smooth:!!ps,endKm:p.endKm,cat:r.slug,noele:!!r.noele,run:!!r.run,world:r.world,up:r.up,locked:!!r.locked,geom:real,detail:ps?10:50,intensity:1};}
  return (zwCache={k,out}).out;}
 function toggleFav(name){const f=favs();f.has(name)?f.delete(name):f.add(name);try{localStorage.setItem(FAV,JSON.stringify([...f]));}catch{}notify();}
 return {CUSTOM,difficulty,quality,slice,climbs,setCal,cal:()=>cal,levelPct,series,pointAt,kmNear,at,load,save,remove,upload,refreshActive,fillPlaces,lang,profile,parseGPX,brouter,byName,favs,toggleFav,cumulative,zwLevelsGeom:slug=>{const r=zw?.routes.find(x=>x.slug===slug);return r?synth(r):null;},thin:(g,n=500)=>simplify(g,n),zwLoad,zwGeom,zwByName,zwState:()=>zwState,zwInfo:()=>zw&&{generated:zw.generated,count:zw.routes.length,url:zw.url},ZW_URL,
  get:id=>items.get(id),list:()=>[...items.values()],active:()=>active,loaded:()=>loaded,on:f=>{listeners.add(f);return()=>listeners.delete(f);}};
})();
