// Host integration with simulated drivers; never opens sockets or accesses a board.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const vm = require('node:vm');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const settle = () => new Promise(resolve => setTimeout(resolve, 10));

async function host(t, failGenerator = false, failConnection = false) {
    const html = fs.readFileSync(path.join(root, 'examples/alpha250/phase-noise-analyzer/web/index.html'), 'utf8');
    const dom = new JSDOM(html, {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const window = dom.window;
    const trace = [];
    const values = Array.from({length: 2}, () => ({carrier: 10e6, modulation: 10e3, phase: 0,
        deviation: 1, duty: .5, seed: 1, waveform: 0, output: false, pm: false}));
    const port = {
        calls: [], fail: failGenerator,
        async init() {
            trace.push('generator-read');
            if (this.fail) throw new Error('Generator unavailable');
            return values.map(() => ({sampleRate: 200e6, phaseWidth: 48, prbsWidth: 31, capabilities: 1023}));
        },
        async settings(channel) { return {...values[channel]}; },
        async set(channel, field, value) { this.calls.push([channel, field, value]); values[channel][field] = value; },
        async restart(channel) { this.calls.push([channel, 'restart']); }
    };
    let initialized;
    const client = {exits: 0};
    window.Client = class {
        constructor(ip, count) { client.ip = ip; client.count = count; }
        init() {
            initialized = failConnection ? Promise.reject(new Error('Connection unavailable')) : Promise.resolve();
            return initialized;
        }
        getDriver() { return {id: 1, getCmds: () => ({get_precision_status: 1, set_phase_precision: 2})}; }
        async readTuple() { return [0, 0, .0015, 1, 1, 0, 0, 0, 0, 0, 0, 5, 20]; }
        exit() { client.exits++; }
    };
    window.Command = (...args) => args;
    window.Imports = class {};
    window.ClockGenerator = class {};
    window.ClockGeneratorApp = class {};
    window.PhaseNoiseAnalyzer = class {
        supportsSampleRate() { return true; }
        async getSamplingFrequency() { return 200e6; }
    };
    window.PhaseNoiseAnalyzerApp = class {
        constructor(document, driver, onError) { client.fail = onError; }
        async init() { trace.push('analyzer-init'); this.nPoints = 16384; }
        setSampleRate() {}
        dispose() { trace.push('analyzer-disposed'); }
    };
    window.PlotBasics = class {};
    window.Plot = class {
        constructor() { trace.push('plot-ready'); }
        markUnavailable(title) { trace.push(title); }
        dispose() { trace.push('plot-disposed'); }
    };
    window.ExportFile = class {};
    window.PhaseModulatorDriver = class { constructor() { return port; } };
    window.$ = () => ({});
    for (const [file, exported] of [
        ['web/phase-noise/phase-precision.ts', 'PhasePrecision'],
        ['web/inputs/digit-input.ts', 'FrequencyInput'],
        ['web/phase-modulator/phase-modulator-widget.ts', 'PhaseModulatorWidget'],
        ['web/phase-noise/sample-rate.ts', 'PnaSampleRate'],
        ['examples/alpha250/phase-noise-analyzer/web/app.ts', null]
    ]) {
        window.eval(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {
            compilerOptions: {target: ts.ScriptTarget.ES2020}
        }).outputText + (exported ? `\nwindow.${exported} = ${exported};` + (exported === 'FrequencyInput' ? '\nwindow.NumberInput = NumberInput;' : '') : ''));
    }
    window.dispatchEvent(new window.Event('HTMLImportsLoaded'));
    await initialized.catch(() => {}); await settle();
    return {window, target: window.document.getElementById('phase-modulator'), port, client, trace};
}

test('analyzer starts before generator discovery; visible panel reads without DAC writes', async t => {
    const h = await host(t);
    assert.deepEqual(h.trace, ['analyzer-init', 'plot-ready', 'generator-read']);
    assert.equal(h.target.closest('details').open, true);
    assert.equal(h.target.querySelectorAll('[data-channel]').length, 2);
    assert.match(h.target.querySelector('.pm-clock').textContent, /200 MS\/s/);
    assert.equal(h.port.calls.length, 0);
    assert.equal(h.window.document.getElementById('instrument-controls').disabled, false);
    assert.equal(h.window.document.getElementById('connection-status').textContent, 'Connected');
    h.target.querySelector('[data-action="refresh"]').click(); await settle();
    assert.equal(h.port.calls.length, 0);
});

test('embedded controls use DAC setters and the 200 MS/s Nyquist limit', async t => {
    const h = await host(t);
    const input = h.target.querySelector('[data-field="carrier"]');
    const enter = value => {
        input.value = value;
        input.dispatchEvent(new h.window.Event('input', {bubbles: true}));
        input.dispatchEvent(new h.window.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    };
    enter('100 MHz'); await settle();
    assert.equal(h.port.calls.length, 0);
    assert.match(h.target.querySelector('.pm-status').textContent, /below 100 MHz/);
    enter('12 MHz'); await settle();
    assert.deepEqual(h.port.calls, [[0, 'carrier', 12e6]]);
    h.target.querySelector('[data-action="output"]').click(); await settle();
    assert.deepEqual(h.port.calls.at(-1), [0, 'output', true]);
    h.window.dispatchEvent(new h.window.Event('pagehide'));
    assert.equal(h.client.exits, 1);
    assert(h.trace.includes('plot-disposed') && h.trace.includes('analyzer-disposed'));
    assert.equal(h.window.document.getElementById('instrument-controls').disabled, true);
    h.window.dispatchEvent(new h.window.Event('pagehide'));
    assert.equal(h.client.exits, 1);
    assert.equal(h.target.querySelector('fieldset').disabled, true);
    enter('13 MHz'); await settle();
    assert.equal(h.port.calls.length, 2);
});

test('connection failure retains locked controls and shows a retry without DAC writes', async t => {
    const h = await host(t, false, true);
    assert.deepEqual(h.trace, []);
    assert.equal(h.port.calls.length, 0);
    assert.equal(h.client.exits, 1);
    for (const id of ['instrument-controls', 'settings-controls', 'plot-controls', 'laser-controls']) {
        assert.equal(h.window.document.getElementById(id).disabled, true);
    }
    assert.equal(h.window.document.getElementById('connection-error').hidden, false);
    assert.equal(h.window.document.getElementById('connection-status').textContent, 'Disconnected');
});

test('generator failure has its own retry while the analyzer plot remains available', async t => {
    const h = await host(t, true);
    assert(h.trace.includes('plot-ready'));
    assert.match(h.target.querySelector('[role="alert"]').textContent, /Generator unavailable/);
    assert.equal(h.port.calls.length, 0);
    h.port.fail = false;
    h.target.querySelector('[data-action="retry"]').click(); await settle();
    assert.equal(h.target.querySelectorAll('[data-channel]').length, 2);
    assert.equal(h.target.querySelector('[role="alert"]'), null);
    assert.equal(h.port.calls.length, 0);
});

test('connection loss after initialization marks readings stale and disables writes without touching DAC settings', async t => {
    const h = await host(t);
    h.window.console.error = () => {};
    h.window.document.body.insertAdjacentHTML('beforeend', '<span id="precision-status">Live</span><span class="tracking-state">Locked</span>');
    h.window.document.querySelector('#decade-values-table').innerHTML = '<tbody><tr><td>1 kHz</td><td>-120 dBc/Hz</td></tr></tbody>';
    h.client.fail(new Error('WebSocket closed'));
    assert.equal(h.window.document.getElementById('connection-status').textContent, 'Disconnected');
    assert.match(h.window.document.getElementById('connection-error-message').textContent, /Connection lost.*stale/);
    assert.equal(h.window.document.getElementById('precision-status').textContent, '—');
    assert.equal(h.window.document.querySelector('.tracking-state').textContent, '—');
    assert.equal(h.window.document.querySelector('#decade-values-table tbody td:last-child').textContent, '—');
    assert.equal(h.window.document.getElementById('average-status').textContent, '—/');
    assert.equal(h.window.document.querySelector('.phase-jitter-span').textContent, '—');
    assert.equal(h.window.document.getElementById('instrument-controls').disabled, true);
    assert.equal(h.target.querySelector('fieldset').disabled, true);
    assert.equal(h.port.calls.length, 0);
    assert(h.trace.includes('Disconnected') && h.trace.includes('plot-disposed'));
    assert.equal(h.client.exits, 1);
});

for (const board of ['alpha250', 'red-pitaya']) {
    test(`${board}: a cached Back/Forward return reconnects after page shutdown`, () => {
        const handlers = new Map();
        const controls = new Map(['instrument-controls', 'settings-controls', 'plot-controls', 'laser-controls']
            .map(id => [id, {disabled: false}]));
        let reloads = 0;
        let exits = 0;
        const context = vm.createContext({Client: class { exit() { exits++; } }});
        const source = fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/app.ts`), 'utf8')
            .replace(/let app = new App\([^;]+;/, '') + '\nthis.TestApp = App;';
        vm.runInContext(ts.transpileModule(source, {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText, context);
        new context.TestApp({
            addEventListener: (name, handler) => handlers.set(name, handler),
            location: {reload() { reloads++; }}
        }, {getElementById: id => controls.get(id)}, 'localhost', {});
        handlers.get('pageshow')({persisted: false});
        assert.equal(reloads, 0, 'ordinary load must not reload in a loop');
        handlers.get('pagehide')();
        assert.equal(exits, 1);
        assert([...controls.values()].every(control => control.disabled));
        handlers.get('pageshow')({persisted: true});
        assert.equal(reloads, 1, 'cached disposed page must start a fresh connection');
    });
}
