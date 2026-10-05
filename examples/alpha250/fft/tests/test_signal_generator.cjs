// Exercise the FFT host with the actual shared widget and simulated hardware.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const settle = () => new Promise(resolve => setTimeout(resolve, 20));

async function host(t, failure = false) {
    const dom = new JSDOM(fs.readFileSync(path.join(root, 'examples/alpha250/fft/web/index.html'), 'utf8'), {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const window = dom.window;
    const trace = [];
    const metadata = Array.from({length: 2}, () => ({sampleRate: 250e6, phaseWidth: 48, prbsWidth: 31, capabilities: 1023}));
    const values = Array.from({length: 2}, () => ({carrier: 10e6, modulation: 10e3, phase: 0,
        deviation: 1, duty: .5, seed: 1, waveform: 0, output: false, pm: false}));
    const port = {
        calls: [], fail: failure,
        async init() { trace.push('generator-read'); if (this.fail) throw new Error('Generator unavailable'); return metadata; },
        async settings(channel) { return {...values[channel], carrier: values[channel].carrier * metadata[channel].sampleRate / 250e6}; },
        async set(channel, field, value) { this.calls.push([channel, field, value]); values[channel][field] = value; },
        async restart(channel) { this.calls.push([channel, 'restart']); }
    };
    const client = {exits: 0};
    window.Client = class { async init() {} exit() { client.exits++; } };
    window.Imports = class {};
    window.FFT = class {
        async init() { trace.push('fft-ready'); this.fft_size = 8192; this.status = {fs: 250e6}; }
    };
    window.FFTApp = class {
        constructor(document, driver, changed) { client.changed = changed; }
        stop() { trace.push('controls-stopped'); }
    };
    window.PrecisionChannelsApp = class { async init() {} setValues() {} dispose() {} };
    for (const name of ['PrecisionDac', 'ClockGenerator', 'ClockGeneratorApp', 'PlotBasics', 'ExportFile']) {
        window[name] = class {};
    }
    window.Plot = class { constructor() { trace.push('plot-ready'); } dispose() { trace.push('plot-stopped'); } };
    window.PhaseModulatorDriver = class { constructor() { return port; } };
    window.$ = () => ({trigger() {}});
    for (const [file, exports] of [
        ['web/phase-modulator/frequency-input.ts', ['FrequencyInput', 'NumberInput']],
        ['web/phase-modulator/phase-modulator-widget.ts', ['PhaseModulatorWidget']],
        ['examples/alpha250/fft/web/app.ts', []]
    ]) {
        window.eval(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {
            compilerOptions: {target: ts.ScriptTarget.ES2020}
        }).outputText + exports.map(name => `\nwindow.${name} = ${name};`).join(''));
    }
    window.dispatchEvent(new window.Event('HTMLImportsLoaded'));
    await settle();
    return {window, target: window.document.getElementById('phase-modulator'), port, client, trace};
}

test('FFT opens the shared PNA controls without writing either DAC and disposes on exit', async t => {
    const h = await host(t);
    assert.deepEqual(h.trace, ['fft-ready', 'plot-ready', 'generator-read']);
    assert.equal(h.target.closest('details').open, true);
    assert.equal(h.target.querySelectorAll('[data-channel]').length, 2);
    assert.equal(h.port.calls.length, 0);
    h.target.querySelector('[data-channel="1"] [data-action="output"]').click(); await settle();
    assert.deepEqual(h.port.calls, [[1, 'output', true]]);
    h.window.dispatchEvent(new h.window.Event('pagehide'));
    assert.equal(h.client.exits, 1);
    assert(h.trace.includes('plot-stopped') && h.trace.includes('controls-stopped'));
    assert.equal(h.target.querySelector('fieldset').disabled, true);
});

test('sampling-rate changes update native readback and validation without DAC writes or lost drafts', async t => {
    const h = await host(t);
    const input = h.target.querySelector('[data-field="carrier"]');
    h.client.changed(200e6); await settle();
    assert.match(h.target.querySelector('.pm-clock').textContent, /200 MS\/s/);
    assert.equal(input.value, '8.000\u2009000');
    assert.equal(h.port.calls.length, 0);
    input.value = '100 MHz';
    input.dispatchEvent(new h.window.Event('input', {bubbles: true}));
    input.dispatchEvent(new h.window.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    await settle();
    assert.equal(h.port.calls.length, 0);
    assert.match(h.target.querySelector('.pm-status').textContent, /below 100 MHz/);
    h.client.changed(250e6); await settle();
    assert.equal(input.value, '100 MHz');
    input.dispatchEvent(new h.window.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    await settle();
    assert.deepEqual(h.port.calls, [[0, 'carrier', 100e6]]);
});

test('generator initialization can be retried while the FFT remains available', async t => {
    const h = await host(t, true);
    assert(h.trace.includes('plot-ready'));
    assert.equal(h.window.document.getElementById('instrument-controls').disabled, false);
    assert.match(h.target.querySelector('[role="alert"]').textContent, /Generator unavailable/);
    h.port.fail = false;
    h.target.querySelector('[data-action="retry"]').click(); await settle();
    assert.equal(h.target.querySelectorAll('[data-channel]').length, 2);
    assert.equal(h.port.calls.length, 0);
});
