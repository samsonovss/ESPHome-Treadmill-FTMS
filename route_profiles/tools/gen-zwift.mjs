// Builds route_profiles/zwift/ from ZwiftMap static streams + zwift-data route list (MIT).
// Usage: npm i zwift-data && node tools/gen-zwift.mjs   (run from route_profiles/)
import { routes } from 'zwift-data';
import fs from 'fs';
const OUT='zwift',MAXP=1500;
const esc=s=>String(s).replace(/[<&"]/g,c=>({'<':'&lt;','&':'&amp;','"':'&quot;'}[c]));
const title=w=>w.replace(/-/g,' ').replace(/\b\w/g,c=>c.toUpperCase());
const index=[],fail=[];
// Elevation every 100 m along the route (m, integer): enough for catalog cards/levels without loading the route file.
const coarse=(dist,alt)=>{const e=[];let j=0;for(let k=0;k<=dist.at(-1);k+=100){while(j<dist.length-2&&dist[j+1]<k)j++;const t=dist[j+1]>dist[j]?(k-dist[j])/(dist[j+1]-dist[j]):0;e.push(Math.round(alt[j]+(alt[j+1]-alt[j])*Math.max(0,Math.min(1,t))));}return e;};
for(const r of routes){if(!r.sports?.includes('running')||!r.stravaSegmentId||r.eventOnly)continue;
 try{const g=async s=>{const x=await fetch(`https://zwiftmap.com/strava-segments/${r.stravaSegmentId}/${s}.json`);if(!x.ok||!/json/.test(x.headers.get('content-type')||''))throw Error(s+' '+x.status);return x.json();};
  const [alt,ll,dist]=await Promise.all([g('altitude'),g('latlng'),g('distance')]);
  if(alt.length!==ll.length||alt.length<2)throw Error('len');
  const step=(dist.at(-1)||0)/MAXP,pts=[];let last=-Infinity;
  for(let i=0;i<ll.length;i++)if(i===ll.length-1||dist[i]-last>=step){pts.push([+ll[i][0].toFixed(5),+ll[i][1].toFixed(5),+(+alt[i]).toFixed(1)]);last=dist[i];}
  const rec={slug:r.slug,name:r.name,world:r.world,km:+r.distance.toFixed(2),up:r.elevation,lap:!!r.lap,locked:!!r.levelLocked,pts};
  fs.writeFileSync(`${OUT}/routes/${r.slug}.json`,JSON.stringify(rec));
  fs.writeFileSync(`${OUT}/gpx/${r.slug}.gpx`,`<?xml version="1.0" encoding="UTF-8"?>\n<gpx version="1.1" creator="zwiftmap.com" xmlns="http://www.topografix.com/GPX/1/1"><trk><name>${esc(`Zwift ${r.name} (${title(r.world)}) ${r.distance.toFixed(1)} km`)}</name><trkseg>\n${ll.map(([a,b],i)=>`<trkpt lat="${a}" lon="${b}"><ele>${alt[i]}</ele></trkpt>`).join('\n')}\n</trkseg></trk></gpx>\n`);
  index.push({slug:r.slug,name:r.name,world:r.world,km:rec.km,up:r.elevation,lap:rec.lap,locked:rec.locked,e:coarse(dist,alt)});
 }catch(e){fail.push(r.slug+': '+e.message);}
 await new Promise(r=>setTimeout(r,120));}
index.sort((a,b)=>a.world.localeCompare(b.world)||a.km-b.km);
fs.writeFileSync(`${OUT}/index.json`,JSON.stringify({v:1,source:'zwiftmap.com + zwift-data',generated:new Date().toISOString().slice(0,10),routes:index},null,0));
console.log('ok',index.length,'fail',fail.length,fail.slice(0,10).join('\n'));
