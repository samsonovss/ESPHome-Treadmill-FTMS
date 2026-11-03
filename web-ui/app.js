'use strict';
// Page shell: sidebar, header (connection, clock, language), settings and the live SSE stream.
// The training page itself (Тренировка · Программы · История) is the embedded ConceptPage.
(() => {
  const $=id=>document.getElementById(id), key='treadmill.ui.v1';
  const defaults=()=>({lang:'ru',collapsed:false});
  let prefs=defaults(), page='main', health='connecting', lastEvent=0;
  try { const p=JSON.parse(localStorage.getItem(key)); if(p){if(['ru','en'].includes(p.lang))prefs.lang=p.lang;if(typeof p.collapsed==='boolean')prefs.collapsed=p.collapsed;if(typeof p.tz==='string')prefs.tz=p.tz;}} catch {prefs=defaults();}
  const sections=['main','settings'];
  const mobile=matchMedia('(max-width: 840px)');
  let drawer=false;
  const paths={main:'M4 17l4-5 4 3 8-10M15 5h5v5',settings:'M4 7h16M4 17h16M8 4v6M16 14v6'};
  function icon(name){const el=document.createElement('span');el.className='nav-icon';el.innerHTML='<svg viewBox="0 0 24 24" aria-hidden="true"><path d="'+paths[name]+'"/></svg>';return el;}
  const t=k=>LOCALES[prefs.lang].messages[k]||k;
  function shell(){
    document.body.classList.toggle('sidebar-collapsed',prefs.collapsed);
    document.body.classList.toggle('nav-open',drawer);
    $('backdrop').hidden=!drawer;
    $('content').inert=drawer; document.querySelector('.skip-link').inert=drawer;
    $('sidebar').inert=mobile.matches&&!drawer;
    $('sidebar').setAttribute('aria-label',t('title'));
    if(drawer){$('sidebar').setAttribute('role','dialog');$('sidebar').setAttribute('aria-modal','true');}else{$('sidebar').removeAttribute('role');$('sidebar').removeAttribute('aria-modal');}
    $('menu-open').setAttribute('aria-label',t('openMenu'));
    $('menu-open').setAttribute('aria-expanded',String(drawer));
    $('sidebar-toggle').setAttribute('aria-label',t(mobile.matches?'closeMenu':prefs.collapsed?'expandMenu':'collapseMenu'));
    $('sidebar-toggle').setAttribute('aria-expanded',String(mobile.matches?drawer:!prefs.collapsed));
  }
  function closeDrawer(restore=true){drawer=false;shell();if(restore&&mobile.matches)$('menu-open').focus();}
  $('menu-open').addEventListener('click',()=>{drawer=true;shell();requestAnimationFrame(()=>$('sidebar-toggle').focus());});
  $('sidebar-toggle').addEventListener('click',()=>{if(mobile.matches)closeDrawer();else{prefs.collapsed=!prefs.collapsed;save();shell();}});
  $('backdrop').addEventListener('click',()=>closeDrawer());
  document.addEventListener('keydown',e=>{if(!drawer)return;if(e.key==='Escape'){e.preventDefault();closeDrawer();}else if(e.key==='Tab'){const items=[$('sidebar-toggle'),...document.querySelectorAll('button[data-page]')],first=items[0],last=items[items.length-1];if(e.shiftKey&&document.activeElement===first){e.preventDefault();last.focus();}else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first.focus();}}});
  mobile.addEventListener('change',()=>{const wasOpen=drawer;drawer=false;shell();if(wasOpen)$('sidebar-toggle').focus();});
  function save(){try{localStorage.setItem(key,JSON.stringify(prefs));}catch{/* private mode: prefs live for this page only */}}
  const controls=new TreadmillControls({send:async path=>{const c=new AbortController(),timer=setTimeout(()=>c.abort(),10000);try{const r=await fetch(path,{method:'POST',signal:c.signal,cache:'no-store'});await r.arrayBuffer();return r;}finally{clearTimeout(timer);}},changed:()=>{}});
  Theme.apply(document.documentElement);  // colours: theme.js (dark only)
  // Fast first paint: last known states from this browser at once; the SSE dump (~200 entities, ~5 s) then refreshes them.
  const cacheKey='treadmill.state.v1';
  try{const c=JSON.parse(localStorage.getItem(cacheKey));if(Array.isArray(c))controls.hydrate(c);}catch{/* no cache yet */}
  const saveStates=()=>{if(!controls.fresh())return;try{localStorage.setItem(cacheKey,JSON.stringify(controls.snapshot()));}catch{/* quota/private mode */}};
  setInterval(saveStates,10000);addEventListener('pagehide',saveStates);
  const settings=DeviceSettings.mount({root:$('settings'),controls,t,lang:()=>prefs.lang});
  controls.changed=()=>settings.render();
  function go(next,focus=true){page=next;render();if(focus)$('content').focus();}
  function renderHealth(){$('health').className='health connection-dot '+health;$('health').setAttribute('aria-label',t(health));$('health').title=t(health);$('no-data').hidden=health==='online';}
  function render(){const ch=document.querySelector('.concept-host');ch?.setAttribute('data-lang',prefs.lang);if(ch&&ch.dataset.tz!==(prefs.tz||''))ch.dataset.tz=prefs.tz||'';document.body.dataset.page=page;document.documentElement.lang=prefs.lang;document.documentElement.dir=LOCALES[prefs.lang].dir;document.title=t('title');document.querySelectorAll('[data-i18n]').forEach(el=>el.textContent=t(el.dataset.i18n));$('nav').setAttribute('aria-label',t('title'));$('heading').textContent=t(page);for(const s of sections)$(s).hidden=s==='settings'&&$(s).slot?false:page!==s;document.querySelectorAll('button[data-page]').forEach(b=>{b.querySelector('.nav-label').textContent=t(b.dataset.page);b.setAttribute('aria-label',t(b.dataset.page));if(b.dataset.page===page)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current');});$('language-toggle').textContent=prefs.lang.toUpperCase();$('language-toggle').lang=prefs.lang;$('language-toggle').setAttribute('aria-label',prefs.lang==='ru'?'Switch to English':'Переключить на русский');$('language-toggle').title=prefs.lang==='ru'?'English':'Русский';shell();renderHealth();settings.render();}
  for(const s of sections){const b=document.createElement('button');b.dataset.page=s;b.className='nav-item';const label=document.createElement('span');label.className='nav-label';b.append(icon(s),label);b.addEventListener('click',()=>{go(s,false);if(drawer){closeDrawer(false);$('content').focus();}});$('nav').append(b);}
  $('language-toggle').addEventListener('click',()=>{prefs.lang=prefs.lang==='ru'?'en':'ru';save();render();});
  // Training page: real data via ConceptLive; Settings are slotted into it.
  {const host=document.createElement('div');host.className='concept-host';$('main').append(host);ConceptPage.mount(host,ConceptLive.api({controls,t}));const set=$('settings');set.slot='settings';host.append(set);}
  // Reconnecting SSE: a stream can end up CLOSED after sleep/network change/ESP reboot, or go silent without an error.
  // ESPHome re-sends a full state dump on every new connection.
  const touch=()=>{lastEvent=Date.now();health='online';};
  function onState(e){try{const data=typeof e.data==='string'?JSON.parse(e.data):e.data;if(typeof data.id!=='string')return;touch();controls.ingest({...data,_seen:Date.now()});if(/^text[-/]time[_ ]zone$/i.test(data.id))boardTz();}catch{/* Ignore malformed events without inventing data. */}}
  let events=null,retryTimer=0,retryDelay=2000;
  const reconnect=()=>{if(retryTimer)return;retryTimer=setTimeout(()=>{retryTimer=0;connect();},retryDelay);retryDelay=Math.min(retryDelay*2,30000);};
  const connect=()=>{events?.close();events=new EventSource('/events');
   events.addEventListener('open',()=>{retryDelay=2000;controls.reconnected();touch();controls.activity();renderHealth();});
   events.addEventListener('ping',()=>{retryDelay=2000;touch();controls.activity();renderHealth();});
   events.addEventListener('state',onState);
   events.addEventListener('error',()=>{health='offline';controls.disconnect();renderHealth();if(events.readyState===EventSource.CLOSED)reconnect();});};
  connect();
  setInterval(()=>{if(lastEvent&&Date.now()-lastEvent>TreadmillControls.STALE_MS&&health==='online'){health='stale';controls.disconnect();renderHealth();events.close();reconnect();}},1000);
  document.addEventListener('visibilitychange',()=>{if(!document.hidden&&(!lastEvent||Date.now()-lastEvent>TreadmillControls.STALE_MS)){clearTimeout(retryTimer);retryTimer=0;retryDelay=2000;connect();}});
  addEventListener('online',()=>{clearTimeout(retryTimer);retryTimer=0;retryDelay=2000;connect();});
  setInterval(()=>controls.tick(),1000);
  addEventListener('pagehide',()=>events?.close());
  // Time zone: stored on the board (text "Time Zone" = "IANA|std|dst|start|end", see 00-core.yaml) so the display
  // clock and workout records use it too. The browser derives the POSIX offsets and DST rules from its own zone data.
  // Old firmware without the entity: browser-only, '' = this browser's zone.
  const tzSel=$('tz'),validTz=z=>{try{new Intl.DateTimeFormat('en',{timeZone:z});return true;}catch{return false;}};if(prefs.tz&&!validTz(prefs.tz))prefs.tz='';
  const tzOff=z=>{try{const p=new Intl.DateTimeFormat('en',{timeZone:z,timeZoneName:'shortOffset'}).formatToParts(new Date()).find(x=>x.type==='timeZoneName');return p?p.value.replace('GMT','UTC'):'';}catch{return '';}};
  const zones=(Intl.supportedValuesOf?Intl.supportedValuesOf('timeZone'):['UTC','Europe/Moscow','Asia/Yekaterinburg']);
  const localTz=Intl.DateTimeFormat().resolvedOptions().timeZone;
  tzSel.append(new Option((prefs.lang==='ru'?'Как в браузере':'Browser')+' ('+localTz+')',''),...zones.map(z=>new Option(z.replace(/_/g,' ')+' · '+tzOff(z),z)));tzSel.value=prefs.tz||'';
  const tzRule=z=>{const f=new Intl.DateTimeFormat('en',{timeZone:z,timeZoneName:'longOffset'});
   const off=ms=>{const m=(f.formatToParts(new Date(ms)).find(x=>x.type==='timeZoneName')?.value||'').match(/([+-])(\d\d):?(\d\d)?/);return m?(m[1]==='-'?-1:1)*(+m[2]*60+(+m[3]||0)):0;}; // minutes east of UTC
   const D=864e5,t0=Date.UTC(new Date().getUTCFullYear(),0,1),ch=[];let prev=off(t0);
   for(let d=1;d<=366;d++){const o=off(t0+d*D);if(o===prev)continue;let a=t0+(d-1)*D,b=t0+d*D;while(b-a>6e4){const m=a+Math.floor((b-a)/12e4)*6e4;if(off(m)===prev)a=m;else b=m;}ch.push({at:b,from:prev,to:o});prev=o;}
   const R=(at,lo)=>{const l=new Date(at+lo*6e4),dd=l.getUTCDate(),dim=new Date(Date.UTC(l.getUTCFullYear(),l.getUTCMonth()+1,0)).getUTCDate();return 'M'+(l.getUTCMonth()+1)+'.'+(dd+7>dim?5:Math.ceil(dd/7))+'.'+l.getUTCDay()+'/'+(l.getUTCHours()*3600+l.getUTCMinutes()*60);};
   const std=Math.min(off(t0),...ch.map(c=>c.to)),dst=Math.max(off(t0),...ch.map(c=>c.to)),s=ch.find(c=>c.from===std&&c.to===dst),e=ch.find(c=>c.from===dst&&c.to===std);
   if(std===dst||!s||!e){const now=off(Date.now());return [z,-now*60,-now*60,'',''].join('|');} // POSIX sign: positive = west
   return [z,-std*60,-dst*60,R(s.at,std),R(e.at,dst)].join('|');};
  const sendTz=z=>{const d=controls.entities.get('text/time_zone');if(!d||!validTz(z))return;try{const v=tzRule(z);if((d.value??d.state)!==v)controls.send('/text/'+encodeURIComponent(d.name)+'/set?value='+encodeURIComponent(v));}catch{}};
  // Board value arrived: it wins over this browser; empty = firmware default, so set it to this browser's zone once.
  // Same zone with different rules (new year, newer zone data) is re-sent so the board stays exact.
  function boardTz(){const d=controls.entities.get('text/time_zone');if(!d)return;const v=String(d.value??d.state??''),z=v.split('|')[0];
   if(!z||!validTz(z)){sendTz(prefs.tz||localTz);return;}
   if(prefs.tz!==z){prefs.tz=z;tzSel.value=z;save();render();tickClock();}sendTz(z);}
  tzSel.addEventListener('change',()=>{prefs.tz=tzSel.value;save();render();tickClock();sendTz(prefs.tz||localTz);});
  const tickClock=()=>{try{$('clock-now').textContent=new Intl.DateTimeFormat(prefs.lang==='ru'?'ru-RU':'en-GB',{hour:'2-digit',minute:'2-digit',timeZone:prefs.tz||undefined}).format(new Date());$('clock-now').title=prefs.tz||Intl.DateTimeFormat().resolvedOptions().timeZone;}catch{}};
  tickClock();setInterval(tickClock,10000);
  render();
})();
