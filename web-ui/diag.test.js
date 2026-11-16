'use strict';
// Offline SSE/DOM harness: no device or network access.
const {readFileSync}=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
let now=1000000;const intervals=[],handlers={};
class ES{constructor(){this.listeners={};ES.instance=this;}addEventListener(k,f){this.listeners[k]=f;}send(k,data){this.listeners[k]?.({data:JSON.stringify(data)});}}
const context={window:{EventSource:ES},localStorage:{getItem:()=>JSON.stringify({b:{heap:999999,block:999999,lowSince:1,lowLogged:true},ev:[]}),setItem(){}},Date:class extends Date{static now(){return now;}},document:{visibilityState:'visible',addEventListener(k,f){handlers[k]=f;}},setInterval:f=>intervals.push(f),setTimeout:()=>1,clearTimeout(){},addEventListener(){},navigator:{userAgent:'Node test'},performance:{now:()=>now},location:{origin:'http://mock'}};
vm.createContext(context);vm.runInContext(readFileSync(__dirname+'/diag.js','utf8')+'\nglobalThis.diag=TMDiag;',context);
const d=context.diag;assert.equal(d.live().heap,undefined);assert.equal(d.status(),'pending');
new context.window.EventSource('/events');const es=ES.instance;es.send('open');
const send=(id,value)=>es.send('state',{id:'sensor/'+id,value});
for(const [k,v]of Object.entries({heap_free:50000,heap_max_block:30000,free_psram:4000000,esp32_wifi_signal:-60,uptime:1000}))send(k,v);
assert.equal(d.status(),'ok');send('memory_trend','decreasing');assert.equal(d.status(),'warning');
send('heap_free',null);assert.equal(d.live().heap,50000);send('heap_free','');assert.equal(d.live().heap,50000);
send('heap_free',20000);for(let i=0;i<61;i++){now+=1000;es.send('ping');}send('heap_max_block',7000);assert.equal(d.status(),'low');assert(d.events().some(e=>e.k==='mem'));
es.send('error');assert.equal(d.status(),'disconnected');assert.equal(d.live().heap,undefined);es.send('ping');assert.equal(d.status(),'pending');send('heap_free',40000);assert.equal(d.live().block,undefined);
send('uptime',1000);const segment=d.history().at(-1).segment;send('uptime',10);send('heap_free',45000);assert(d.history().at(-1).segment>segment);assert.equal(d.live().block,undefined);
now+=160000;intervals[0]();assert.equal(d.status(),'disconnected');es.send('ping');assert.equal(d.live().heap,undefined);
for(let i=0;i<1500;i++){now+=10;send('heap_free',40000+i);}assert.equal(d.history().length,1200);assert(!d.report().includes('undefined'));assert(d.report().includes('chart: browser-only'));
d.clear();assert.equal(d.events().length,0);assert.equal(d.history().length,1200);
console.log('PASS: cached/pending, normal, trend, sustained low, invalid data, offline/reconnect, reboot gaps, stale, bounded history, report, clear');
