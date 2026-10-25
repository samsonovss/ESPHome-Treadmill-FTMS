'use strict';
// Offline FIT activity encoder. Profile fields follow recorder/live/treadmill_fit_export.py.
const TreadmillFit=(()=>{
  const epoch=631065600, types={enum:[0,1,255],u8:[2,1,255],u16:[132,2,65535],s16:[131,2,32767],u32:[134,4,4294967295],u32z:[140,4,0]};
  function crc(bytes){let c=0;for(const b of bytes){c^=b;for(let i=0;i<8;i++)c=(c&1)?(c>>>1)^0xa001:c>>>1;}return c;}
  function number(v){if(v===null||v===undefined||String(v).trim()==='')return null;const n=Number(v);return Number.isFinite(n)?n:null;}
  function timestamp(v){const n=typeof v==='string'&&/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/.test(v)?Date.parse(v)/1000:number(v);if(n===null||!Number.isInteger(n)||n<epoch||n>=epoch+4294967295)throw Error('fitTime');return n;}
  function parseCSV(text,interrupted=false){if(interrupted&&!text.endsWith("\n"))text=text.slice(0,text.lastIndexOf("\n")+1);const lines=[];let row=[],s='',q=false;for(let i=0;i<text.length;i++){const c=text[i];if(c==='"'){if(q&&text[i+1]==='"'){s+='"';i++;}else q=!q;}else if(c===','&&!q){row.push(s);s='';}else if((c==='\n'||c==='\r')&&!q){if(c==='\r'&&text[i+1]==='\n')i++;row.push(s);if(row.some(x=>x!==''))lines.push(row);row=[];s='';}else s+=c;}if(q)throw Error('historyInvalid');if(s||row.length){row.push(s);lines.push(row);}const head=lines.shift();if(!head||!['timestamp','status','incline_pct','elapsed_s'].every(k=>head.includes(k)))throw Error('historyInvalid');const rows=lines.map(r=>{if(r.length!==head.length)throw Error('historyInvalid');return Object.fromEntries(head.map((k,i)=>[k,r[i]]));});
  // 17.10 compact CSV: an empty cell = previous value; empty time_s = elapsed_s; empty timestamp = last one + elapsed delta
  const keep=['status','program','zone','incline_level','incline_pct','target_zone'],prev={};let ts=null,el0=0;
  for(const r of rows){for(const k of keep){if(r[k]==='')r[k]=prev[k]??'';else prev[k]=r[k];}const el=+r.elapsed_s;
   if(r.time_s==='')r.time_s=r.elapsed_s;
   if(r.timestamp){const t=Date.parse(r.timestamp);if(Number.isFinite(t)){ts=t;el0=el;}}else if(ts!=null&&Number.isFinite(el))r.timestamp=new Date(ts+Math.round(el-el0)*1000).toISOString().replace(/\.\d+Z$/,'Z');}
  return rows;}
  function build(rows,summary){
    if(!rows.length||summary.wall_time_valid!==true)throw Error('fitTime');
    const start=timestamp(summary.start),end=timestamp(summary.end);
    const active=number(summary.duration_sec);let elapsed=number(summary.elapsed_sec);
    // 24.10: records on Treadmill Time — elapsed = workout time + pauses (the recorder clock started ~1 s before the treadmill one: 1:00 run read 1:01)
    if(summary.time_base==='treadmill'&&active!==null&&elapsed!==null){let paused=0;for(let i=1;i<rows.length;i++)if(rows[i-1].status==='paused'){const d=number(rows[i].elapsed_s)-number(rows[i-1].elapsed_s);if(d>0)paused+=d;}elapsed=Math.min(elapsed,active+paused);}
    if(end<start||active===null||elapsed===null||active<0||elapsed<active)throw Error('historyInvalid');
    let previous=start;for(const r of rows){const ts=timestamp(r.timestamp);if(ts<previous||ts>end||!['running','paused'].includes(r.status))throw Error('fitTime');previous=ts;}
    const data=[],defs={};
    const push=(v,size)=>{for(let i=0;i<size;i++)data.push((v/2**(8*i))&255);};
    function define(local,global,fields){defs[local]=fields;data.push(64|local,0,0);push(global,2);data.push(fields.length);for(const [field,type]of fields)data.push(field,types[type][1],types[type][0]);}
    function msg(local,values){data.push(local);for(const [field,type]of defs[local]){const [,size,invalid]=types[type];let value=values[field];if(value===null||value===undefined||!Number.isFinite(value))value=invalid;else{value=Math.round(value);const low=type==='s16'?-32768:0,high=type==='s16'?32766:type==='u32z'?4294967295:invalid-1;if(value<low||value>high)value=invalid;}if(value<0)value+=2**(size*8);push(value,size);}}
    const scaled=(v,scale)=>number(v)===null?null:number(v)*scale;
    define(0,0,[[0,'enum'],[1,'u16'],[2,'u16'],[3,'u32z'],[4,'u32']]);
    define(1,21,[[253,'u32'],[0,'enum'],[1,'enum'],[4,'u8']]);
    define(2,20,[[253,'u32'],[3,'u8'],[5,'u32'],[6,'u16'],[9,'s16']]);
    // Correct FIT profile summary fields (Python's legacy lap calorie/speed fields were shifted).
    define(3,19,[[253,'u32'],[0,'enum'],[1,'enum'],[2,'u32'],[7,'u32'],[8,'u32'],[9,'u32'],[11,'u16'],[13,'u16'],[14,'u16'],[15,'u8'],[16,'u8'],[23,'enum'],[24,'enum'],[25,'enum'],[39,'enum']]);
    define(4,18,[[253,'u32'],[2,'u32'],[7,'u32'],[8,'u32'],[9,'u32'],[11,'u16'],[14,'u16'],[15,'u16'],[16,'u8'],[17,'u8'],[5,'enum'],[6,'enum'],[25,'u16'],[26,'u16']]);
    define(5,34,[[253,'u32'],[0,'u32'],[1,'u16'],[2,'enum'],[3,'enum'],[4,'enum']]);
    msg(0,{0:4,1:255,2:1,3:0x54454e01,4:start-epoch});
    const event=(ts,type)=>msg(1,{253:ts-epoch,0:0,1:type,4:0});
    let running=true;event(start,0);
    for(const r of rows){const now=r.status==='running';if(now!==running){event(timestamp(r.timestamp),now?0:1);running=now;}msg(2,{253:timestamp(r.timestamp)-epoch,3:scaled(r.heart_rate,1),5:scaled(r.distance_km,100000),6:scaled(r.speed_kmh,1000/3.6),9:scaled(r.incline_pct,100)});}
    event(end,4);
    const speeds=rows.map(r=>number(r.speed_kmh)).filter(x=>x!==null),hrs=rows.map(r=>number(r.heart_rate)).filter(x=>x!==null&&x>0);
    const distance=number(summary.distance_km),avg=number(summary.avg_speed_kmh)??(active>0&&distance!==null?distance*3600/active:null),max=speeds.length?speeds.reduce((a,b)=>Math.max(a,b),0):null;
    const avgHr=number(summary.avg_heart_rate)??(hrs.length?hrs.reduce((a,b)=>a+b,0)/hrs.length:null),maxHr=number(summary.max_heart_rate)??(hrs.length?hrs.reduce((a,b)=>Math.max(a,b),0):null);
    const common={253:end-epoch,2:start-epoch,7:elapsed*1000,8:active*1000,9:scaled(summary.distance_km,100000),11:scaled(summary.calories,1)};
    // Laps by program phase (record "phases" [[timer s,"W|R|H|L|C|0"],…]): warm-up / cool-down / rest / active, as Garmin watches write them. No phases → one lap.
    const KI={W:2,C:3,L:1},cuts=[[0,0]];for(const p of Array.isArray(summary.phases)?summary.phases:[]){const t=number(p?.[0]),k=KI[p?.[1]]??0;if(t===null)continue;if(t<=0)cuts[0][1]=k;else if(t>cuts.at(-1)[0]&&t<active)cuts.push([t,k]);}/* 24.10: every recorded phase start is a lap — each fartlek segment, even two Easy in a row (was merged by intensity) */
    const groups=cuts.map(([t,k])=>({t,k,rows:[]}));for(const r of rows){const t=number(r.time_s);let g=0;if(t!==null)while(g+1<groups.length&&groups[g+1].t<=t)g++;groups[g].rows.push(r);}
    const laps=groups.filter(g=>g.rows.length),num=(r,k)=>number(r?.[k]);
    laps.forEach((g,i)=>{const n=laps[i+1],a=timestamp(g.rows[0].timestamp),b=n?timestamp(n.rows[0].timestamp):end,tb=n?n.t:active,timer=Math.max(0,tb-(i?g.t:0));
      const d0=i?num(g.rows[0],'distance_km'):0,d1=n?num(n.rows[0],'distance_km'):distance,dist=d0!==null&&d1!==null?Math.max(0,d1-d0):null;
      const c0=i?num(g.rows[0],'calories'):0,c1=n?num(n.rows[0],'calories'):number(summary.calories),cal=laps.length===1?number(summary.calories):c0!==null&&c1!==null?Math.max(0,c1-c0):null;
      const sp=g.rows.map(r=>num(r,'speed_kmh')).filter(x=>x!==null),hr=g.rows.map(r=>num(r,'heart_rate')).filter(x=>x!==null&&x>0);
      const lavg=laps.length===1?avg:timer>0&&dist!==null?dist*3600/timer:null;
      msg(3,{253:b-epoch,0:9,1:1,2:a-epoch,7:(laps.length===1?elapsed:g.rows.some(r=>r.status==='paused')?Math.max(timer,b-a):timer)*1000/* 24.10: no pause in the lap → elapsed = its Treadmill time (wall-clock seconds rounded the last lap to 31) */,8:(laps.length===1?active:timer)*1000,9:scaled(laps.length===1?distance:dist,100000),11:cal,13:scaled(lavg,1000/3.6),14:scaled(laps.length===1?max:sp.length?Math.max(...sp):null,1000/3.6),15:laps.length===1?avgHr:hr.length?hr.reduce((x,y)=>x+y,0)/hr.length:null,16:laps.length===1?maxHr:hr.length?Math.max(...hr):null,23:g.k,24:n?0:7,25:1,39:1});});
    msg(4,{...common,14:scaled(avg,1000/3.6),15:scaled(max,1000/3.6),16:avgHr,17:maxHr,5:1,6:1,25:0,26:laps.length});
    msg(5,{253:end-epoch,0:active*1000,1:1,2:0,3:26,4:4});
    const out=new Uint8Array(14+data.length+2),view=new DataView(out.buffer);out.set([14,16,0,0]);view.setUint32(4,data.length,true);out.set([46,70,73,84],8);view.setUint16(12,crc(out.subarray(0,12)),true);out.set(data,14);view.setUint16(out.length-2,crc(out.subarray(0,-2)),true);return out;
  }
  return {crc,parseCSV,build};
})();
if(typeof module!=='undefined')module.exports=TreadmillFit;
