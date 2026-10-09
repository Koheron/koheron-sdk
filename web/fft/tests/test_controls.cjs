// Real FFT controls and shared telemetry poller with independent simulated I/O.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../..');
const settle = async () => { for (let i = 0; i < 12; i++) await Promise.resolve(); };

function host(t, readBoard) {
    const dom = new JSDOM('<input class="fft-input" data-command="setInputChannel" value="1" type="radio"><select class="fft-select" data-command="setFFTWindow"><option value="1">Hann</option></select>' +
        Array.from({length:4}, (_, i) => `<span class="precision-adc-span" data-channel="${i}"></span>`).join(''), {runScripts:'outside-only', pretendToBeVisual:true});
    const w = dom.window, d = w.document, timers = new Map(), errors = [], rendered = [], writes = [];
    let id = 0, controls = 0, now = 250;
    Object.defineProperty(w.performance, 'now', {value: () => now});
    w.setTimeout = (callback, delay) => {timers.set(++id, {callback, delay}); return id;};
    w.clearTimeout = key => timers.delete(key);
    w.console.error = (...args) => errors.push(args);
    const files = ['web/instrument/events.ts', 'web/instrument/poller.ts', 'web/power-monitor/readout.ts', 'web/temperature-sensor/readout.ts', 'web/fft/controls/fft-app.ts'];
    w.eval(ts.transpileModule(files.map(file => fs.readFileSync(path.join(root, file), 'utf8')).join('\n'),
        {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText + '\nwindow.FFTApp = FFTApp;');
    const driver = {
        async getControlParameters() { controls++; return {fs:250e6, channel:1, window_index:1, clkIndex:'0', dds_freq:[]}; },
        setInputChannel(value) { writes.push(value); }
    };
    if (readBoard) driver.getBoardParameters = readBoard;
    const app = new w.FFTApp(d, driver, undefined, values => rendered.push(Array.from(values)));
    t.after(() => {app.dispose(); w.close();});
    const tick = async delay => {
        const entry = Array.from(timers.entries()).find(([, timer]) => timer.delay === delay);
        assert(entry, `Missing ${delay} ms timer`);
        timers.delete(entry[0]); now += delay; entry[1].callback(); await settle();
    };
    return {w,d,app,timers,errors,rendered,writes,tick,get controls(){return controls;}};
}
const boardValues = {supplyValues:[], temperatures:[], adcValues:[.1,.2,.3,.4], dacValues:[1,2,3,4]};

test('slow telemetry leaves acquisition controls responsive and never overlaps', async t => {
    let finish, reads = 0;
    const h = host(t, () => {reads++; return new Promise(resolve => {finish = resolve;});});
    await settle();
    assert.equal(h.controls, 1); assert.equal(reads, 1);
    for (let i=0; i<6; i++) await h.tick(250);
    assert.equal(h.controls, 7); assert.equal(reads, 1);
    const input = h.d.querySelector('input'); input.dispatchEvent(new h.w.Event('change'));
    assert.deepEqual(h.writes, [1]);
    finish(boardValues); await settle();
    assert.deepEqual(h.rendered, [[1,2,3,4]]);
    assert.equal(h.d.querySelector('.precision-adc-span').textContent, '100.0000');
    Object.defineProperty(h.d, 'hidden', {value:true, configurable:true});
    await h.tick(1000); assert.equal(reads, 1);
    Object.defineProperty(h.d, 'hidden', {value:false, configurable:true});
    await h.tick(1000); assert.equal(reads, 2);
    h.app.dispose(); finish({...boardValues, dacValues:[9,9,9,9]}); await settle();
    assert.equal(h.rendered.length, 1); // Discard a reply arriving after exit.
    assert(!Array.from(h.timers.values()).some(timer => timer.delay === 1000));
});

test('telemetry failure retries independently of the control loop', async t => {
    let reads = 0;
    const h = host(t, async () => {if (++reads === 1) throw new Error('Telemetry unavailable'); return boardValues;});
    await settle();
    assert.equal(h.errors.length, 1);
    await h.tick(250); assert.equal(h.controls, 2);
    await h.tick(1000);
    assert.equal(reads, 2); assert.deepEqual(h.rendered, [[1,2,3,4]]);
    assert.equal(h.errors.length, 1);
});

test('boards without telemetry schedule only acquisition controls', async t => {
    const h = host(t); await settle(); await h.tick(250);
    assert.equal(h.controls, 2); assert.equal(h.errors.length, 0);
    assert(!Array.from(h.timers.values()).some(timer => timer.delay === 1000));
});
