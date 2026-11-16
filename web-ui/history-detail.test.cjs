'use strict';
// Focused actual-JS integration: cached rows + delayed personal routes + list metadata gap.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync(__dirname+'/preview/concept/index.html','utf8'),adapter=fs.readFileSync(__dirname+'/concept-live.js','utf8');
const open=src.slice(src.indexOf('let openBusy=false'),src.indexOf('// Bulk delete progress'));
const zone=src.slice(src.indexOf('function zoneTimes('),src.indexOf('/* 19.10: the summary'));
const memo=src.slice(src.indexOf('function renderSummary(){'),src.indexOf('function renderSummary0(){'));
const c=vm.createContext({console,Date,Map,Math,Error});
vm.runInContext(`let historyOwner='p1',historyEpoch=1,window={treadmillHistoryOwner:'p1'},LOCALRUN=null,RP=null,view='hist',viewing=null,summary=null,setup=null,mode='manual',histNote='',histMeta=new Map(),rowsCache=new Map(),data=[],Z=[300,310,320,330],draws=0,listeners=[],routes={};
const live={zones:null},PROGRAM_MODE={},progBase=x=>x,progName=x=>x,renderHist=()=>{},saveCache=()=>{},rpStop=()=>{},draw=()=>draws++;
const historyMatches=m=>m?.profile?.id===historyOwner,zoneOf=hr=>hr==null?-1:hr<Z[0]?0:4;
const API={history:{rows:async()=>{throw Error('cached CSV expected')},summary:async()=>{throw Error('list summary expected')}},routes:()=>routes,routesLib:{on:f=>listeners.push(f)}};
`+open+zone+`
const LOC=()=> 'ru',TZO=()=> 'UTC';let range=0,renders=0;const $=()=>({children:[],querySelector:()=>true});function renderSummary0(){renders++}
`+memo,c);
const run=s=>vm.runInContext(s,c),id='w0000000002';
const record=p=>({id,profile:{id:p,hr_max:181},start:'2026-11-16T12:00:00Z',end:'2026-11-16T12:17:00Z',program:'None',route:p==='p1'?'Anton route':'Test route',zone_sec:p==='p1'?[0,60,120,180,240,420]:[0,0,0,0,0,1020]});
(async()=>{
 run(`rowsCache.set('${id}',[[0,116,6,0,0,0,1],[1020,116,6,0,1.7,100,1]])`);
 for(const p of ['p1','p2','p1']){
  c.record=record(p);run(`historyEpoch++;historyOwner=window.treadmillHistoryOwner='${p}';summary=null;viewing=null;routes={};histMeta.set('${id}',record)`);
  await run(`openHist('${id}',true)`);assert.equal(run('data.length'),2);assert.equal(run('summary.record.profile.id'),p);
  // Later pagination may temporarily unpublish list metadata; detail must remain coherent.
  run(`histMeta.delete('${id}')`);
  assert.deepEqual(Array.from(run('zoneTimes(summary)')),record(p).zone_sec.slice(1));
  assert.equal(run('summary.route'),undefined);
  run(`routes[record.route]={geom:[[55,60,100],[55.01,60.01,120]],points:[[0,0],[1.7,2]],detail:25};listeners.forEach(f=>f())`);
  assert.equal(run('summary.route.name'),record(p).route);assert.equal(run('summary.mode'),'map');assert.equal(run('mode'),'map');assert.equal(run('summary.route.geom.length'),2);
  assert.match(run('summary.name'),new RegExp(record(p).route));assert.deepEqual(Array.from(run('zoneTimes(summary)')),record(p).zone_sec.slice(1));
 }
 run(`const keep=summary.record;summary.record={...keep,zone_sec:[0,0,0,0,0,0]};const zero=zoneTimes(summary);summary.record=keep;`);assert.deepEqual(Array.from(run('zero')),[0,0,0,0,0],'recorded zeros remain authoritative');
 // Actual memo key must redraw same-sized records and changed historical title.
 run(`renderSummary();const n=renders;summary.name='Changed route';renderSummary()`);assert(run('renders===n+1'));
 run(`const n2=renders;summary.record={...summary.record,zone_sec:[0,61,120,180,240,420]};renderSummary()`);assert(run('renders===n2+1'));
 // Route clear notification arrives before the concept profile listener: no other-owner hydration.
 run(`window.treadmillHistoryOwner='p2';routes={'Anton route':{geom:[[1,2],[3,4]],points:[[0,0]]}};const before=summary.route;listeners.forEach(f=>f())`);assert(run('summary.route===before'));
 // Execute the real adapter cache branch: ids must await async publication, not race its caller.
 const text=adapter.slice(adapter.indexOf('    listCache:null'),adapter.indexOf('\n    async assign'));
 let release;const pending=new Promise(r=>release=r);const a=vm.createContext({Map,Set,Error,TreadmillHistory:require('./history.js'),hc:{check:async()=>({history_token:'same'})},pending});
 vm.runInContext(`const obj={meta:new Map(),${text}};obj.listCache={token:'same',ids:['${id}'],meta:new Map()};let published=false;`,a);
 let finished=false;const task=vm.runInContext('obj.ids(async()=>{await pending;published=true})',a).then(()=>finished=true);await new Promise(r=>setImmediate(r));assert(!finished,'cached list must not resolve before publication');release();await task;assert(vm.runInContext('published',a));
 console.log('PASS actual JS p1→p2→p1: cached CSV, delayed owner route hydration/name/mode, recorded zone seconds through metadata gap, early profile-notify guard; cached adapter awaits publication');
})().catch(e=>{console.error(e);process.exitCode=1});
