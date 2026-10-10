// Zwift lists many roads twice: "X" (shared) and "X Run" (running menu) with the same road and elevation.
// Keep one per group — the running-only one if present (that's the name runners see), else the first.
// Usage: node tools/dedupe.mjs   (run from route_profiles/, after gen-zwift.mjs and fill-flat.mjs)
import fs from 'fs';
const D='zwift',idx=JSON.parse(fs.readFileSync(`${D}/index.json`)),R=idx.routes;
const same=(a,b)=>{if(a.world!==b.world||Math.abs(a.km-b.km)>0.15||Math.abs(a.e.length-b.e.length)>4)return false;const n=Math.min(a.e.length,b.e.length);for(let k=0;k<n;k++)if(Math.abs(a.e[k]-b.e[k])>3)return false;return true;};
const drop=new Set();
for(let x=0;x<R.length;x++){if(drop.has(x))continue;const g=[x];for(let y=x+1;y<R.length;y++)if(!drop.has(y)&&same(R[x],R[y]))g.push(y);if(g.length<2)continue;
 const keep=g.find(k=>R[k].run)??g[0];for(const k of g)if(k!==keep){drop.add(k);console.log('drop',R[k].name,'→ keep',R[keep].name);}}
// Reverse direction ("… Reverse", "… CCW", "… Rev") is dropped when the forward route is in the catalog.
const base=n=>n.replace(/\s*(reverse|ccw|rev)\b/ig,'').replace(/\s+run$/i,'').trim().toLowerCase();
for(let k=0;k<R.length;k++){if(drop.has(k)||!/reverse|\bccw\b|\brev\b/i.test(R[k].name))continue;const fw=R.findIndex((x,j)=>j!==k&&!drop.has(j)&&x.world===R[k].world&&!/reverse|\bccw\b|\brev\b/i.test(x.name)&&base(x.name)===base(R[k].name));
 if(fw>=0){drop.add(k);console.log('drop reverse',R[k].name,'→ keep',R[fw].name);}}
for(const k of drop)for(const f of [`${D}/routes/${R[k].slug}.json`,`${D}/gpx/${R[k].slug}.gpx`])if(fs.existsSync(f))fs.unlinkSync(f);
idx.routes=R.filter((_,k)=>!drop.has(k));fs.writeFileSync(`${D}/index.json`,JSON.stringify(idx));console.log('left',idx.routes.length);
