// A visible window may delay requestAnimationFrame while network data continues.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const context = vm.createContext({assert, console});
vm.runInContext(ts.transpileModule(fs.readFileSync(path.resolve(__dirname, '../web/plot/plot.ts'), 'utf8'), {
    compilerOptions: {target: ts.ScriptTarget.ES2020}
}).outputText, context);
vm.runInContext(`
let now = 0, nextId = 1, draws = 0;
const raf = new Map(), timers = new Map();
globalThis.performance = {now: () => now};
globalThis.window = {
    requestAnimationFrame(fn) { const id = nextId++; raf.set(id, fn); return id; },
    cancelAnimationFrame(id) { raf.delete(id); },
    setTimeout(fn, delay) { const id = nextId++; timers.set(id, {fn, due: now + delay}); return id; },
    clearTimeout(id) { timers.delete(id); }
};
const fields = new Map();
const plot = Object.assign(Object.create(Plot.prototype), {
    running: true, paused: false, animation: 0, lastFrameTime: -Infinity,
    waitMs: 0, renderMs: 0, renderedFrames: 0,
    document: {hidden: false, getElementById(id) {
        if (!fields.has(id)) fields.set(id, {dataset: {}, textContent: ''});
        return fields.get(id);
    }},
    displaySpectrum() { draws++; }, setStatus() {},
    pending: {psd: new Float32Array([1]), status: {}}
});
const tick = time => {
    now = time;
    for (const [id, event] of Array.from(timers)) {
        if (event.due <= now) { timers.delete(id); event.fn(); }
    }
};
plot.requestDraw();
assert.equal(raf.size, 1); assert.equal(timers.size, 1);
tick(1); // Browser never delivered its animation callback.
assert.equal(draws, 1); assert.equal(raf.size, 0); assert.equal(timers.size, 0);
for (let i = 1; i <= 60; i++) {
    now = 1 + i * 17;
    plot.pending = {psd: new Float32Array([i]), status: {}};
    plot.requestDraw(); plot.requestDraw();
    assert.equal(timers.size, 1); // No queue despite repeated acquisitions.
    tick(now + 1);
}
assert.equal(draws, 61); // Approximately 60 Hz even when rAF is starved.
now += 20; plot.pending = {psd: new Float32Array([99]), status: {}};
plot.requestDraw();
Array.from(raf.values())[0](now); // Normal animation callback wins.
assert.equal(draws, 62); assert.equal(timers.size, 0); assert.equal(raf.size, 0);
plot.pending = {psd: new Float32Array([100]), status: {}};
plot.requestDraw(); plot.setPaused(true); tick(now + 1000);
assert.equal(draws, 62); assert.equal(timers.size, 0); assert.equal(raf.size, 0);
plot.paused = false; plot.document.hidden = true; plot.requestDraw();
assert.equal(timers.size, 0); assert.equal(raf.size, 0);
`, context);
console.log('Delayed animation fallback, frame cap, cancellation and latest-frame queue: PASS');
