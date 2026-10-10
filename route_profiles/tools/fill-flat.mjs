// Some running segments on ZwiftMap carry a constant altitude (e.g. Epic Run = 1 m everywhere).
// Zwift roads are shared between routes of one world, so take each point's elevation from the nearest
// point of the world's routes that do have elevation (≤ 15 m away). Rewrites routes/, gpx/ and index.json e[].
// Usage: node tools/fill-flat.mjs   (run from route_profiles/, after gen-zwift.mjs)
import fs from 'fs';
const D='zwift',R=d=>d*Math.PI/180,MAXD=15,CELL=0.0003;
const hav=(a,b)=>{const dl=R(b[0]-a[0]),dn=R(b[1]-a[1]),h=Math.sin(dl/2)**2+Math.cos(R(a[0]))*Math.cos(R(b[0]))*Math.sin(dn/2)**2;return 12742000*Math.asin(Math.min(1,Math.sqrt(h)));};
const idx=JSON.parse(fs.readFileSync(`${D}/index.json`)),load=s=>JSON.parse(fs.readFileSync(`${D}/routes/${s}.json`));
const flat=r=>{const e=r.pts.map(p=>p[2]);return Math.max(...e)-Math.min(...e)<1;};
const all=idx.routes.map(r=>({meta:r,rec:load(r.slug)})),grid={};
for(const {meta,rec} of all){if(flat(rec))continue;const g=grid[meta.world]??={};for(const p of rec.pts){const k=Math.floor(p[0]/CELL)+','+Math.floor(p[1]/CELL);(g[k]??=[]).push(p);}}
// Nearest road point; with prev elevation known, prefer continuity (bridges/underpasses: same lat/lon, other height).
const near=(g,p,prev=null)=>{const a=Math.floor(p[0]/CELL),b=Math.floor(p[1]/CELL);let best=null,bs=Infinity;for(let i=-1;i<=1;i++)for(let j=-1;j<=1;j++)for(const q of g[(a+i)+','+(b+j)]??[]){const d=hav(p,q);if(d>MAXD)continue;const sc=d+(prev==null?0:Math.abs(q[2]-prev)*4);if(sc<bs){bs=sc;best=q;}}return best;};
const coarse=(pts)=>{const d=[0];for(let i=1;i<pts.length;i++)d.push(d[i-1]+hav(pts[i-1],pts[i]));const e=[];let j=0;for(let k=0;k<=d.at(-1);k+=50){while(j<d.length-2&&d[j+1]<k)j++;const t=d[j+1]>d[j]?(k-d[j])/(d[j+1]-d[j]):0;e.push(Math.round((pts[j][2]+(pts[j+1][2]-pts[j][2])*Math.max(0,Math.min(1,t)))*10)/10);}return e;};
for(const {meta,rec} of all){if(!flat(rec))continue;const g=grid[meta.world]??{};let hit=0,prev=null;const ele=rec.pts.map(p=>{const q=near(g,p,prev);if(q){hit++;prev=q[2];return q[2];}return null;});
 const cov=hit/rec.pts.length;if(cov<0.85){console.log('skip',meta.slug,(cov*100).toFixed(0)+'%');meta.noele=true;continue;}
 // gaps → linear between known neighbours
 for(let i=0;i<ele.length;i++)if(ele[i]==null){let a=i-1;while(a>=0&&ele[a]==null)a--;let b=i+1;while(b<ele.length&&ele[b]==null)b++;ele[i]=a<0?ele[b]:b>=ele.length?ele[a]:ele[a]+(ele[b]-ele[a])*(i-a)/(b-a);}
 rec.pts=rec.pts.map((p,i)=>[p[0],p[1],+ele[i].toFixed(1)]);fs.writeFileSync(`${D}/routes/${meta.slug}.json`,JSON.stringify(rec));meta.e=coarse(rec.pts);delete meta.noele;
 const gp=`${D}/gpx/${meta.slug}.gpx`;if(fs.existsSync(gp)){let pv=null;const s=fs.readFileSync(gp,'utf8').replace(/<trkpt lat="([-\d.]+)" lon="([-\d.]+)"><ele>[-\d.]+<\/ele><\/trkpt>/g,(m,la,lo)=>{const q=near(g,[+la,+lo],pv);if(!q)return m;pv=q[2];return `<trkpt lat="${la}" lon="${lo}"><ele>${q[2]}</ele></trkpt>`;});fs.writeFileSync(gp,s);}
 let up=0;for(let i=1;i<meta.e.length;i++)up+=Math.max(0,meta.e[i]-meta.e[i-1]);console.log('fixed',meta.slug,(cov*100).toFixed(0)+'%','up≈'+up,'(zwift '+meta.up+')');}
fs.writeFileSync(`${D}/index.json`,JSON.stringify(idx));
