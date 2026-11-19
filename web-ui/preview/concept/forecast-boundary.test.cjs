const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const html = fs.readFileSync(`${__dirname}/index.html`, 'utf8');
const functions = html.slice(html.indexOf('function fkFuture(now)'), html.indexOf('// Past program phases'));
let checks = 0;
for (const state of ['running', 'paused']) {
  const ctx = vm.createContext({
    API: { settings: () => ({ fplan: '' }) },
    live: { state, phase: 'cool', prem: 0, plan: { cool: 10 } },
    HIIT: { cool: 10 }, rPhase: () => ({ left: 0 }), fkParse: () => [], FK_REST: 5,
  });
  vm.runInContext(functions, ctx);
  for (const mode of ['hiit', 'fartlek']) {
    const condition = html.match(new RegExp(`if\\((mode==='${mode}'&&HF_.*?)\\)\\{`))[1];
    ctx.mode = mode; ctx.range = null; ctx.summary = null;
    for (const left of [0, 1]) {
      ctx.rPhase = () => ({ left });
      ctx.HF_ = vm.runInContext(`${mode === 'hiit' ? 'hiitFuture' : 'fkFuture'}(100)`, ctx);
      assert.equal(ctx.HF_.length, left ? 1 : 0);
      // Execute the actual chart gate and its formerly unsafe tail access.
      assert.equal(vm.runInContext(`(${condition}) ? HF_.at(-1).b : null`, ctx), left ? 101 : null);
      checks++;
    }
    ctx.HF_ = null;
    assert.equal(vm.runInContext(`(${condition}) ? HF_.at(-1).b : null`, ctx), null);
    checks++;
  }
}
console.log(`PASS: ${checks} forecast boundary checks`);
