'use strict';
// Canonical definitions remain in the web page; no separately maintained numeric copies.
const fs=require('fs'),vm=require('vm');
const source=fs.readFileSync('web-ui/preview/concept/index.html','utf8');
const read=n=>vm.runInNewContext(source.match(new RegExp(n+'=(\\[[\\s\\S]*?\\]);'))[1]);
const q=s=>JSON.stringify(s||'');let rows=[];
for(const [kind,name]of [['hiit','HIIT_BUILTIN'],['fartlek','FK_BUILTIN']])for(const p of read(name))rows.push(` {${[q(kind),q(p.name),q(p.plan),q(p.wmode),q(p.cmode),p.hi??60,p.lo??60,p.cycles??8,p.warm??3,p.cool??3,p.low??'NAN',p.high??'NAN',!!p.hiitManual,p.warm!=null||p.cool!=null].join(',')}}`);
fs.writeFileSync('esphome/treadmill_builtin_presets.h',`// GENERATED from canonical web builtins by tools/nextion-history/generate-builtins.cjs; do not edit.\n#pragma once\n#include <cmath>\nnamespace treadmill_builtin_presets {\nstruct Preset {const char *kind,*name,*plan,*wmode,*cmode;int hi,lo,cycles;float warm,cool,low,high;bool manual,hasWC;};\ninline const Preset items[]={\n${rows.join(',\n')}\n};\n}\n`);
