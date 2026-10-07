const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const board = process.env.PNA_SAMPLE_RATE_BOARD || 'alpha250';
const settle = () => new Promise(resolve => setTimeout(resolve, 10));

async function fixture(t, supported = true) {
    const dom = new JSDOM(fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/index.html`), 'utf8'), {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const w = dom.window, calls = [], rendered = [], errors = [];
    let poll;
    w.setTimeout = callback => { poll = callback; return 1; };
    w.clearTimeout = () => { poll = undefined; };
    const driver = {
        rate: 250e6, accepted: true,
        supportsSampleRate: () => supported,
        async getParameters() { return {fs: 1e6, cic_rate: 100}; },
        async getSamplingFrequency() { return this.rate; },
        async setSamplingFrequency(rate) { calls.push(rate); if (this.accepted) this.rate = rate; return this.accepted; }
    };
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/phase-noise/sample-rate.ts'), 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText + '\nwindow.PnaSampleRate = PnaSampleRate;');
    const widget = new w.PnaSampleRate(w.document, driver, rate => rendered.push(rate), error => errors.push(error));
    t.after(() => widget.dispose());
    await widget.init();
    const select = w.document.querySelector('#sample-rate');
    w.document.querySelector('#instrument-controls').disabled = false;
    w.document.querySelector('#settings-controls').disabled = false;
    const choose = async rate => { select.value = String(rate); select.dispatchEvent(new w.Event('change')); await settle(); };
    return {w, widget, driver, select, choose, calls, rendered, errors, poll: async () => { poll(); await settle(); }};
}

test('sample rate starts from hardware and a successful transition updates metadata', async t => {
    const h = await fixture(t);
    assert.equal(h.select.value, '250000000');
    assert.equal(h.w.document.querySelector('#sample-rate-label').textContent, '250');
    assert.deepEqual(h.calls, []);
    await h.choose(200e6);
    assert.deepEqual(h.calls, [200e6]);
    assert.deepEqual(h.rendered, [250e6, 200e6]);
    assert.equal(h.w.document.querySelector('#sample-rate-label').textContent, '200');
    assert.equal(h.select.disabled, false);
    assert.equal(h.w.document.querySelector('#sample-rate-status').textContent, '');
});

test('a rejected transition restores the actual selection and explains the rejection', async t => {
    const h = await fixture(t);
    h.driver.accepted = false;
    await h.choose(200e6);
    assert.equal(h.select.value, '250000000');
    assert.deepEqual(h.rendered, [250e6]);
    assert.match(h.w.document.querySelector('#sample-rate-status').textContent, /rejected/);
    assert.equal(h.errors.length, 0);
});

test('another client changing rate updates the header and editor metadata', async t => {
    const h = await fixture(t);
    h.driver.rate = 200e6;
    await h.poll();
    assert.equal(h.select.value, '200000000');
    assert.deepEqual(h.rendered, [250e6, 200e6]);
    assert.deepEqual(h.calls, []);
    h.widget.dispose();
    await h.choose(250e6);
    assert.deepEqual(h.calls, []);
});

test('older firmware shows its actual clock with switching disabled', async t => {
    const h = await fixture(t, false);
    assert.equal(h.select.disabled, true);
    assert.equal(h.w.document.querySelector('#sample-rate-label').textContent, '200');
    assert.deepEqual(h.calls, []);
});
