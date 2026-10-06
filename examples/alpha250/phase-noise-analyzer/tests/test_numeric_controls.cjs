// Exercise the real analyzer control bindings with an in-memory driver.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const settle = () => new Promise(resolve => setTimeout(resolve, 15));
async function fixture(t, board = 'alpha250') {
    const project = 'examples/alpha250/phase-noise-analyzer/';
    const dom = new JSDOM(fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/index.html`), 'utf8'), {runScripts: 'outside-only'});
    const w = dom.window; t.after(() => w.close());
    w.document.querySelector('#dds-frequency').innerHTML = fs.readFileSync(path.join(root, project + 'web/dds-frequency/dds-frequency.html'), 'utf8').replace(/<\/?template[^>]*>/g, '');
    const referenceClock = w.document.querySelector('#reference-clock');
    if (referenceClock) referenceClock.innerHTML = fs.readFileSync(path.join(root, project + 'web/clock-generator/reference-clock.html'), 'utf8').replace(/<\/?template[^>]*>/g, '');
    w.requestAnimationFrame = () => 0;
    for (const [file, exports] of [['web/phase-modulator/frequency-input.ts', ['FrequencyInput', 'NumberInput']], [project + 'web/phase-noise-analyzer-app.ts', ['PhaseNoiseAnalyzerApp']]]) {
        w.eval(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + exports.map(name => `\nwindow.${name} = ${name};`).join(''));
    }
    const parameters = {data_size: 16384, fs: 5e6, channel: 0, cic_rate: 20, fft_navg: 1, fdds0: 10e6, fdds1: 10e6, analyzer_mode: 'RF', interferometer_delay: 1e-9, clkIndex: 2};
    const calls = [];
    const tracking = {enabled: false, bandwidth: .1, effectiveBandwidth: .1, maxStep: .05, maxCorrection: 100,
        nominal0: 10e6, nominal1: 10e6, correction0: 0, correction1: 0,
        error0: NaN, error1: NaN, locked0: false, locked1: false};
    const average = {count: 0, target: 1};
    const driver = {
        async getAverageStatus() { return {...average}; },
        async getParameters() { return {...parameters}; },
        async getTrackingParameters() { return {...tracking}; },
        setTrackingEnabled(value) { calls.push(['tracking', value]); tracking.enabled = value; },
        async getMeasurements() { return {carrier_power: 0, phase_jitter: 0, time_jitter: 0, freq_lo: 1e3, freq_hi: 1e6}; },
        setCicRate(value) { calls.push(['cic', value]); parameters.cic_rate = value; },
        setFFTNavg(value) { calls.push(['navg', value]); parameters.fft_navg = Math.min(value, 100); },
        setLocalOscillator(channel, value) { calls.push(['lo', channel, value]); parameters['fdds' + channel] = value; tracking['nominal' + channel] = value; },
        setInterferometerDelay(value) { calls.push(['delay', value]); parameters.interferometer_delay = value; },
        setAnalyzerMode(value) { calls.push(['laser', value]); parameters.analyzer_mode = value ? 'laser' : 'RF'; }
    };
    const app = new w.PhaseNoiseAnalyzerApp(w.document, driver); t.after(() => app.dispose());
    await app.init(); await settle();
    w.document.querySelector('#instrument-controls').disabled = false;
    w.document.querySelector('#laser-controls').disabled = false;
    w.document.querySelector('#plot-controls').disabled = false;
    const key = (input, key) => input.dispatchEvent(new w.KeyboardEvent('keydown', {key, bubbles: true}));
    const enter = (input, value) => { input.value = value; input.dispatchEvent(new w.Event('input', {bubbles: true})); key(input, 'Enter'); };
    return {w, app, parameters, tracking, average, driver, calls, key, enter};
}

test('analyzer numbers select a digit by default and wheel works anywhere on the page', async t => {
    const h = await fixture(t); const {w, calls} = h;
    assert.equal(calls.length, 0);
    assert.equal(w.document.querySelectorAll('input[type="number"]').length, 0);
    assert.equal(w.document.querySelectorAll('input[role="spinbutton"]').length, 5);
    const input = w.document.querySelector('.cic-rate-input'); input.focus();
    assert.equal(input.selectionEnd - input.selectionStart, 1);
    const wheel = new w.WheelEvent('wheel', {deltaY: -40, bubbles: true, cancelable: true});
    w.document.querySelector('#plot-placeholder').dispatchEvent(wheel); await settle();
    assert.equal(wheel.defaultPrevented, true); assert.deepEqual(calls, [['cic', 22]]);
    h.key(input, 'ArrowLeft'); h.key(input, 'ArrowUp');
    await new Promise(resolve => setTimeout(resolve, 120));
    assert.deepEqual(calls.at(-1), ['cic', 32]);
    const count = calls.length; h.app.dispose();
    w.document.body.dispatchEvent(new w.WheelEvent('wheel', {deltaY: -40, bubbles: true, cancelable: true})); await settle();
    assert.equal(calls.length, count);
});

test('Red Pitaya accepts odd CIC rates while ALPHA250 requires even rates', async t => {
    const redp = await fixture(t, 'red-pitaya');
    const input = redp.w.document.querySelector('.cic-rate-input');
    input.focus(); redp.enter(input, '67'); await settle();
    assert.deepEqual(redp.calls, [['cic', 67]]);
    redp.key(input, 'ArrowUp');
    await new Promise(resolve => setTimeout(resolve, 120));
    assert.deepEqual(redp.calls.at(-1), ['cic', 68]);
    const alpha = await fixture(t);
    const evenInput = alpha.w.document.querySelector('.cic-rate-input');
    evenInput.focus(); alpha.enter(evenInput, '67'); await settle();
    assert.equal(alpha.calls.length, 0);
    alpha.enter(evenInput, '68'); await settle();
    assert.deepEqual(alpha.calls, [['cic', 68]]);
});

test('tracking is opt-in and telemetry does not overwrite nominal LO edits', async t => {
    const h = await fixture(t);
    const toggle = h.w.document.querySelector('.tracking-enabled-input');
    assert.equal(toggle.checked, false);
    assert.equal(h.calls.length, 0);
    toggle.checked = true; toggle.dispatchEvent(new h.w.Event('change'));
    assert.deepEqual(h.calls, [['tracking', true]]);
    h.tracking.correction0 = -.037;
    h.parameters.fdds0 = 10e6 - .037;
    await h.app.updateControls();
    assert.equal(Number(h.w.document.querySelector('.dds-input0').value.replace(/\s/g, '')), 10);
    const input = h.w.document.querySelector('.dds-input0'); input.focus();
    input.value = '10.000001'; input.dispatchEvent(new h.w.Event('input', {bubbles: true}));
    h.tracking.correction0 = -.045;
    await h.app.updateControls();
    assert.equal(input.value, '10.000001');
    h.key(input, 'Enter'); await settle();
    assert.deepEqual(h.calls.at(-1), ['lo', 0, 10e6 + 1]);

});

test('integer entry applies once, rejects incomplete/invalid values, and Escape restores accepted value', async t => {
    const h = await fixture(t); const input = h.w.document.querySelector('.plot-navg-input'); input.focus();
    h.enter(input, '10'); h.key(input, 'Enter'); await settle();
    assert.deepEqual(h.calls, [['navg', 10]]);
    for (const value of ['', '1.5', '0', '101', 'NaN']) { h.enter(input, value); await settle(); }
    assert.equal(h.calls.length, 1);
    h.key(input, 'Escape'); assert.equal(input.value, '10');
    h.w.document.querySelector('.interferometer-delay').focus(); await settle();
    assert.equal(h.calls.length, 1);
});

test('LO unit entry tunes only the reference and accepts 100 MHz; laser delay stays disabled until enabled', async t => {
    const h = await fixture(t); const lo = h.w.document.querySelector('.dds-input0'); lo.focus();
    h.enter(lo, '12 kHz'); await settle();
    assert.deepEqual(h.calls, [['lo', 0, 12000]]);
    assert.equal(lo.parentElement.querySelector('.lo-unit').value, 'kHz');
    assert.equal(h.w.document.querySelector('.dds-set0'), null);
    h.enter(lo, '100 MHz'); await settle(); assert.deepEqual(h.calls.at(-1), ['lo', 0, 100e6]);
    h.enter(lo, '100.000001 MHz'); await settle(); assert.equal(h.calls.length, 2);
    const delay = h.w.document.querySelector('.interferometer-delay');
    assert.equal(delay.disabled, true); h.enter(delay, '20'); await settle(); assert.equal(h.calls.length, 2);
    const laser = h.w.document.querySelector('.laser-mode-input'); laser.checked = true; laser.dispatchEvent(new h.w.Event('change'));
    delay.focus(); h.enter(delay, '20'); await settle();
    assert.deepEqual(h.calls.slice(-2), [['laser', 1], ['delay', 20e-9]]);
});

test('CSV uses the displayed frame metadata when editable controls have changed', async t => {
    const h = await fixture(t); const {w} = h;
    const file = 'examples/alpha250/phase-noise-analyzer/web/export-file/';
    w.document.querySelector('#export-file').innerHTML = fs.readFileSync(path.join(root, file + 'export-file.html'), 'utf8').replace(/<\/?template[^>]*>/g, '');
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, file + 'export-file.ts'), 'utf8'), {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.ExportFile = ExportFile;');
    let blob;
    w.Blob = Blob;
    w.URL.createObjectURL = value => { blob = value; return 'blob:http://test/download'; };
    w.URL.revokeObjectURL = () => {};
    w.HTMLAnchorElement.prototype.click = () => {};
    new w.ExportFile(w.document, {frameStatus: {...h.parameters}, yLabel: 'Phase noise (dBc/Hz)', plot_data: [[1e3, -130]],
        smooth_plot_data: [[1e3, -130]], phase_psd: new Float32Array([2e-13])});
    h.enter(w.document.querySelector('.dds-input0'), '10.000123 MHz'); await settle();
    h.enter(w.document.querySelector('.dds-input1'), '12 kHz'); await settle();
    h.enter(w.document.querySelector('.plot-navg-input'), '100'); await settle();
    w.document.querySelector('.export-data').click();
    const csv = await blob.text();
    assert.match(csv, /"LO 0 frequency \(Hz\)",10000000\n/);
    assert.match(csv, /"LO 1 frequency \(Hz\)",10000000\n/);
    assert.match(csv, /"Decimation rate",20\n/);
    assert.match(csv, /"Averaging window \(spectra\)",1\n/);
});


test('average progress distinguishes waiting, initial fill and a full rolling window', async t => {
    const {w, app, average, driver} = await fixture(t);
    const status = w.document.getElementById('average-status');
    assert.equal(w.document.getElementById('average-progress'), null);
    assert.equal(status.nextElementSibling, w.document.querySelector('.plot-navg-input'));
    average.target = 100;
    await app.updateAverageProgress();
    assert.equal(status.textContent, '0/');
    assert.match(status.title, /0 of 100.*Waiting/);
    average.count = 3;
    await app.updateAverageProgress();
    assert.equal(status.textContent, '3/');
    assert.match(status.title, /3 of 100.*filling/);
    assert.equal(status.dataset.state, 'filling');
    average.count = 100;
    await app.updateAverageProgress();
    assert.equal(status.textContent, '100/');
    assert.match(status.title, /Full window/);
    assert.equal(status.dataset.state, 'full');
    average.count = average.target = 1;
    await app.updateAverageProgress();
    assert.equal(status.textContent, '1/');
    driver.getAverageStatus = async () => { throw new Error('Disconnected'); };
    await app.updateAverageProgress();
    assert.equal(status.textContent, '—/');
    assert.equal(status.title, 'Average progress unavailable');
    app.dispose();
    driver.getAverageStatus = async () => ({count: 2, target: 10});
    await app.updateAverageProgress();
    assert.equal(status.textContent, '—/');
});
