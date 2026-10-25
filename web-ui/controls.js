'use strict';
// Native REST only; UI never publishes firmware state or retries commands.
class TreadmillControls {
  constructor({send,now=()=>Date.now(),changed=()=>{}}){this.send=send;this.now=now;this.changed=changed;this.reset();}
  reset(){this.lastActivity=0;this.entities=new Map();this.online=false;this.board=null;this.pending=new Map();this.notice='';this.changed();}
  disconnect(){this.online=false;this.entities.clear();this.board=null;this.notice=this.pending.size?'uncertain':'';this.pending.clear();this.changed();}
  ingest(d){const split=d.id?.match(/^([^/-]+)[/-](.*)$/);if(!split)return;const domain=d.domain||split[1],name=d.name||split[2];const key=domain+'/'+name.toLowerCase().replace(/ /g,'_');const old=this.entities.get(key)||{};this.entities.set(key,{...old,...d,domain,name});this.online=true;this.lastActivity=this.now();
    if(key==='text_sensor/ui_control_state'){const m=String(d.value??d.state).match(/^(running|paused|stopped)\|(manual|auto)(?:\|\d+)?$/);if(m)this.board={state:m[1],mode:m[2],at:this.now()};else this.board=null;}
    for(const [k,p] of this.pending){if(this.now()>p.at && this.confirmed(p)){this.pending.delete(k);this.notice='confirmed';}}
    this.changed();
  }
  // Liveness = any SSE traffic (state events or web_server ping every ~10 s), like the Silvia UI.
  // UI Control State itself is published only on change.
  activity(){this.lastActivity=this.now();}
  fresh(){return this.online&&!!this.board&&this.now()-this.lastActivity<TreadmillControls.STALE_MS;}
  state(){return this.fresh()?this.board.state:'unknown';}
  traits(k){const d=this.entities.get('number/set_'+k);if(!d)return null;const min=Number(d.min_value),max=Number(d.max_value),step=Number(d.step);return [min,max,step].every(Number.isFinite)&&step>0&&max>=min?{min,max,step}:null;}
  allowed(k){if(!this.fresh())return false;if(k==='stop')return this.entities.has('button/stop_treadmill')&&!this.pending.has('stop');const number=['speed','incline'].includes(k);if([...this.pending.keys()].some(x=>!number||x!==k))return false;const s=this.state();if(['speed','incline'].includes(k))return s==='running'&&!!this.traits(k)&&this.entities.has('sensor/treadmill_'+k)&&(k!=='speed'||this.board.mode==='manual');const expected={start:'stopped',pause:'running',resume:'paused'};return s===expected[k]&&this.entities.has('button/'+{start:'start_treadmill',pause:'pause_training',resume:'resume_training'}[k])&&(k!=='start'||this.entities.has('select/workout_program'));}
  confirmed(p){if(!this.fresh())return false;if(p.target)return this.state()===p.target;const d=this.entities.get('sensor/treadmill_'+p.key);return d?.value!==null&&d?.value!==undefined&&d?.value!==''&&Number.isFinite(Number(d.value))&&Math.abs(Number(d.value)-p.value)<0.001&&d!==p.before;}
  async command(k,raw){if(!this.allowed(k))return false;let value;const number=['speed','incline'].includes(k);if(number){const tr=this.traits(k);if(String(raw).trim()===''){this.notice='invalidNumber';this.changed();return false;}value=Number(raw);if(!Number.isFinite(value)||value<tr.min||value>tr.max||Math.abs((value-tr.min)/tr.step-Math.round((value-tr.min)/tr.step))>0.00001){this.notice='invalidNumber';this.changed();return false;}}
    const d=this.entities.get(number?'number/set_'+k:'button/'+{start:'start_treadmill',pause:'pause_training',resume:'resume_training',stop:'stop_treadmill'}[k]);const path='/'+d.domain+'/'+encodeURIComponent(d.name)+'/'+(number?'set?value='+encodeURIComponent(value):'press');
    if(k==='stop')this.pending.clear();const p={key:k,value,target:{start:'running',resume:'running',pause:'paused',stop:'stopped'}[k],at:this.now(),before:this.entities.get('sensor/treadmill_'+k)};this.pending.set(k,p);this.notice='pending';this.changed();
    try{const r=await this.send(path);if(!r.ok)throw Error('HTTP');if(this.pending.get(k)===p){this.notice='accepted';this.changed();}}catch{if(this.pending.get(k)===p){this.pending.delete(k);this.notice='failed';this.changed();}}return true;
  }
  tick(){for(const [k,p] of this.pending)if(this.now()-p.at>=12000){this.pending.delete(k);this.notice='timeout';}this.changed();}
}
TreadmillControls.STALE_MS=15000;
if(typeof module!=='undefined')module.exports=TreadmillControls;
