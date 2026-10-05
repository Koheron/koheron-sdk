// Actual Alpha15 adapters, templates and shared workspace; simulated board I/O.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const project = 'examples/alpha15/signal-analyzer';
const settle = () => new Promise(resolve => setTimeout(resolve, 25));

async function host(t, failure = false) {
    const dom = new JSDOM(fs.readFileSync(path.join(root, project, 'web/index.html'), 'utf8'), {
        runScripts: 'outside-only', pretendToBeVisual: true
    });
    t.after(() => dom.window.close());
    const w = dom.window, d = w.document;
    const state = {writes: [], reads: [], exits: 0, window: 1, channel: 0, operation: 0, ranges: [0, 0], reference: 2, errors: []};
    w.console.error = (...args) => state.errors.push(args);
    const timers = new Map(), frames = new Map();
    let id = 0, time = 0;
    w.setTimeout = fn => { timers.set(++id, fn); return id; };
    w.clearTimeout = key => timers.delete(key);
    w.requestAnimationFrame = fn => { frames.set(++id, fn); return id; };
    w.cancelAnimationFrame = key => frames.delete(key);
    const flush = async () => {
        await settle();
        const queued = Array.from(frames.values()); frames.clear();
        for (const fn of queued) { fn(time += 17); }
        await settle();
    };
    const canvas = {setTransform() {}, clearRect() {}, fillRect() {}, strokeRect() {}, fillText() {},
        save() {}, restore() {}, translate() {}, rotate() {}, drawImage() {}, putImageData() {}, beginPath() {}, moveTo() {}, lineTo() {}, stroke() {},
        createImageData(width, height) { return {data: new Uint8ClampedArray(width * height * 4)}; }};
    w.HTMLCanvasElement.prototype.getContext = () => canvas;
    w.Imports = class {
        constructor(document) {
            const assets = [`${project}/web`, `${project}/web/fft`, `${project}/web/plot`, `${project}/web/adc-range`,
                `${project}/web/clock-generator`, `${project}/web/precision-channels`, `${project}/web/temperature-sensor`, `${project}/web/power-monitor`,
                'web/fft', 'web/fft/controls', 'web/fft/plot', 'web/fft/export-file', 'web/plot-basics'];
            for (const link of document.querySelectorAll('link[rel="import"]')) {
                const file = assets.map(dir => path.join(root, dir, link.getAttribute('href'))).find(fs.existsSync);
                assert(file, `Missing import ${link.getAttribute('href')}`);
                const template = new JSDOM(fs.readFileSync(file, 'utf8')).window.document.querySelector('template');
                const target = document.getElementById(link.dataset.parent);
                assert(target, `Missing mount ${link.dataset.parent}`);
                target.append(document.importNode(template.content, true));
            }
        }
    };
    w.Command = (driver, name, ...args) => ({driver, name, args});
    w.Client = class {
        async init() { if (failure) throw new Error('Board offline'); }
        exit() { state.exits++; }
        getDriver(name) { return {id: name, getCmds: () => new Proxy({}, {get: (_, key) => key})}; }
        send(command) { state.writes.push(command); }
        async readUint32(command) {
            state.reads.push(command);
            switch (command.name) {
                case 'get_fft_size': return 8192;
                case 'get_window_index': return state.window;
                case 'input_range': return state.ranges[command.args[0]];
                case 'get_reference_clock': return state.reference;
                default: throw new Error(command.name);
            }
        }
        async readTuple(command, format) {
            state.reads.push(command);
            if (command.driver === 'Decimator') {
                assert.equal(format, 'ffffIII');
                return [15e6 / 32, 15e6 / 512, 8192 / (15e6 / 32), 8192 / (15e6 / 512), 16, 256, 8192];
            }
            assert.equal(format, 'dIIddd');
            return [15e6, state.channel, state.operation, 8192 ** 2 / 4, 8192 * .375, 1.5 / 8192];
        }
        async readFloat32Array(command) {
            state.reads.push(command);
            switch (command.name) {
                case 'read_psd': { const data = new w.Float32Array(4096).fill(1e-16); data[1311] = 1e-8; return data; }
                case 'get_dac_values': return new w.Float32Array([.1, .2, .3, .4]);
                case 'get_temperatures': return new w.Float32Array([30, 40, 50]);
                case 'get_supplies_ui': return new w.Float32Array([.1, 12, .02, 3.3]);
                default: throw new Error(command.name);
            }
        }
        async readFloat64Vector(command) { state.reads.push(command); return new w.Float64Array(4097).fill(1e-16); }
    };
    let range = {from: 10, to: 7.5e6}, drawn;
    w.$ = () => ({on() {}, off() {}, trigger() { range = {from: 10, to: 7.5e6}; }});
    w.PlotBasics = class {
        constructor() {}
        enableSpectrumReduction() {} enableBatchedLines() {} setLinY() {} setLogX() {} needsRedraw() { return false; }
        setRangeX(from, to) { range = {from, to}; } getRangeX() { return range; }
        setVisibleRangeX(from, to) { range = {from, to}; }
        redraw(data, count, peak, label, cb) { drawn = {data, count, peak, label}; cb(); }
    };
    const files = ['web/fft/driver.ts', 'web/fft/controls/fft-app.ts', 'web/fft/controls/precision-channels.ts',
        'web/phase-modulator/frequency-input.ts', 'web/fft/plot/spectrum-history.ts', 'web/fft/plot/spectrum-views.ts',
        'web/fft/plot/plot.ts', 'web/fft/export-file/export-file.ts', 'web/fft/workspace.ts',
        ...['adc-range/ltc2387.ts', 'clock-generator/clock-generator.ts', 'precision-channels/precision-dac.ts',
            'temperature-sensor/temperature-sensor.ts', 'power-monitor/power-monitor.ts', 'decimator.ts', 'fft.ts', 'board-controls.ts', 'app.ts'].map(file => `${project}/web/${file}`)];
    w.eval(ts.transpileModule(files.map(file => fs.readFileSync(path.join(root, file), 'utf8')).join('\n'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText + '\nwindow.workspace = app;');
    w.dispatchEvent(new w.Event('HTMLImportsLoaded'));
    await flush();
    assert.equal(state.errors.length, failure ? 1 : 0, 'Unexpected browser error');
    t.after(() => w.dispatchEvent(new w.Event('pagehide')));
    return {w, d, state, timers, flush, app: w.workspace, get drawn() { return drawn; }};
}

test('Alpha15 mounts shared UX with four inputs, voltage units, ranges and read-only precision startup', async t => {
    const h = await host(t);
    assert.equal(h.d.querySelector('h1').textContent, 'Signal analyzer');
    assert.equal(h.d.querySelector('.fft-generator').hidden, true);
    assert.equal(h.d.getElementById('instrument-controls').disabled, false);
    assert.equal(h.d.getElementById('input-range').hidden, false);
    assert.equal(h.d.getElementById('reference-clock').hidden, false);
    assert.equal(h.d.querySelectorAll('.fft-input').length, 4);
    assert.equal(h.d.querySelectorAll('.precision-dac-input').length, 4);
    assert.equal(h.state.writes.length, 0);
    assert.deepEqual(Array.from(h.d.querySelectorAll('.precision-dac-input'), input => input.value), ['100', '200', '300', '400']);
    assert.equal(h.d.querySelectorAll('input[type="range"]').length, 0);
    assert.equal(h.d.getElementById('connection-status').textContent, 'Live spectrum');
    assert.equal(h.drawn.label, 'Voltage noise (dBV/√Hz)');
});

test('multiband grid is zero based, strictly ordered, covers RF, and uses each band ENBW', async t => {
    const h = await host(t), fft = h.app.fft, plot = h.app.plot;
    const grid = fft.status.spectrum, data = plot.plot_data;
    assert.equal(grid.frequencies[0], 0);
    assert.equal(grid.frequencies[1], 15e6 / 512 / 8192);
    for (let i = 1; i < grid.frequencies.length; i++) assert(grid.frequencies[i] > grid.frequencies[i - 1]);
    assert.equal(grid.frequencies.at(-1), 4095 * 15e6 / 8192);
    assert.equal(data.length, grid.frequencies.length);
    assert.equal(h.d.getElementById('bin-spacing').textContent, '3.58 Hz / 57.2 Hz / 1.83 kHz');
    assert.equal(h.drawn.peak[0], 1311 * 15e6 / 8192);
    assert(Math.abs(plot.convertValue(1e-12, 'nv-rtHz', fft.status) - 1000) < 1e-9);
    assert.equal(plot.convertValue(1e-12, 'dbv-rtHz', fft.status), -120);
    assert(Math.abs(plot.convertValue(1e-12, 'dBV', fft.status, 0) - 10 * Math.log10(1e-12 * 1.5 * 15e6 / 512 / 8192)) < 1e-9);
    assert(Math.abs(plot.convertValue(1e-12, 'dBV', fft.status, data.length - 1) - 10 * Math.log10(1e-12 * 1.5 * 15e6 / 8192)) < 1e-9);
    const lowReads = h.state.reads.filter(c => c.name === 'spectral_density1').length;
    await fft.read_psd();
    assert.equal(h.state.reads.filter(c => c.name === 'spectral_density1').length, lowReads);
    const view = plot.views;
    assert(Math.abs(view.frequencyPosition(.5, {from: 10, to: 1e6}) - Math.sqrt(1e7)) < 1e-9);
    const spans = view.columns({from: 10, to: 7.5e6}, 800);
    assert(spans.every(span => span.first >= 0 && span.last < data.length));
});

test('channel/window commands preserve Alpha15 semantics; ranges reset history and references own their grid', async t => {
    const h = await host(t), fft = h.app.fft, plot = h.app.plot;
    fft.setInputChannel(3); fft.setFFTWindow(2);
    assert.deepEqual(h.state.writes.map(c => [c.driver, c.name, ...c.args]), [
        ['FFT', 'set_operation', 1], ['FFT', 'select_adc_channel', 2],
        ['FFT', 'set_fft_window', 2], ['Decimator', 'set_fft_window', 2]
    ]);
    plot.captureReference();
    const reference = plot.referenceStatus;
    const epoch = plot.history.epoch;
    h.state.channel = 2; h.state.operation = 1; h.state.ranges = [1, 0]; h.state.reference = 0;
    await fft.getControlParameters();
    assert.equal(fft.status.channel, 3); assert.equal(fft.status.clkIndex, '0');
    assert.equal(plot.channelLabel(fft.status), 'ADC 0 + 1');
    plot.history.add(await fft.read_psd(), fft.status, 1);
    assert(plot.history.epoch > epoch);
    assert.equal(reference.channel, 0); assert.equal(reference.inputRanges[0], 2.048);
    h.d.getElementById('pause-display').click();
    assert.equal(h.d.getElementById('pause-display').textContent, 'Resume');
    assert.equal(h.d.getElementById('connection-status').textContent, 'Display paused');
    h.w.dispatchEvent(new h.w.Event('pagehide'));
    assert.equal(h.state.exits, 1); assert.equal(plot.running, false);
});

test('Alpha15 exports full voltage spectra and history with Hz and input range metadata', async t => {
    const h = await host(t), plot = h.app.plot;
    let output;
    h.app.exportFile.download = async blob => { output = await blob.text(); };
    // Use host Blob so text() is available in jsdom.
    h.w.Blob = Blob;
    h.app.exportFile.exportData(); await settle();
    assert(output.includes('Frequency (Hz),Voltage noise (dBV/√Hz)'));
    assert(output.startsWith('Koheron ALPHA15 Signal analyzer'));
    assert(h.app.exportFile.frameLabel(plot.frameStatus).includes('2.048 / 2.048 V ranges'));
    assert(output.includes('ADC 0 range (V),2.048'));
    assert(!output.includes('DDS 0 (Hz)'));
    assert(output.includes(String(plot.plot_data.at(-1)[0])));
    plot.views.mode = 'spectrogram'; h.app.exportFile.exportData(); await settle();
    assert(output.includes('Age (s) / Frequency (Hz)'));
    plot.views.mode = 'density'; plot.unit = 'dBV'; h.app.exportFile.exportData(); await settle();
    assert(output.includes('Frequency (Hz),Voltage (dBV),Count'));
});

test('failed connection exposes Retry and tears down without board writes', async t => {
    const h = await host(t, true);
    assert.equal(h.d.getElementById('connection-error').hidden, false);
    assert.equal(h.d.getElementById('connection-status').textContent, 'Disconnected');
    assert.equal(h.d.getElementById('instrument-controls').disabled, true);
    assert.equal(h.state.writes.length, 0); assert.equal(h.state.exits, 1);
});

test('logarithmic rendering reduction preserves detail and peaks across frequency decades', () => {
    const vm = require('node:vm');
    const context = vm.createContext({assert});
    vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(root, 'web/plot-basics/plot-basics.ts'), 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText, context);
    vm.runInContext(`
        const bins = Array.from({length: 12000}, (_, i) => [10 * Math.pow(10, i / 2000), -150 + Math.sin(i)]);
        bins[500][1] = -30;
        bins[7000][1] = -40;
        const reduced = PlotBasics.reduceSpectrum(bins, 10, 1e7, 600, true);
        assert(reduced.includes(bins[500]));
        assert(reduced.includes(bins[7000]));
        for (let decade = 1; decade <= 6; decade++) {
            const points = reduced.filter(row => row[0] >= 10 ** decade && row[0] < 10 ** (decade + 1));
            assert(points.length >= 190, 'Log screen columns must retain LF detail');
        }
        assert(reduced.length <= 1202);
    `, context);
});


test('density and spectrogram render the nonuniform grid with voltage bandwidth per band', async t => {
    const h = await host(t), plot = h.app.plot, views = plot.views;
    const canvas = h.d.getElementById('history-canvas');
    Object.defineProperties(canvas, {clientWidth: {value: 400}, clientHeight: {value: 300}});
    const selector = h.d.getElementById('spectrum-view');
    selector.value = 'density'; selector.dispatchEvent(new h.w.Event('change'));
    const unit = h.d.querySelector('.unit-input[value="dBV"]');
    unit.checked = true; unit.dispatchEvent(new h.w.Event('change'));
    assert.equal(views.mode, 'density');
    assert.equal(h.d.getElementById('history-level-unit').textContent, 'dBV');
    const code = plot.history.constructor.code(1e-16);
    const lowRow = views.densityRow(code, 'dBV', 1, 256);
    const highRow = views.densityRow(code, 'dBV', plot.plot_data.length - 1, 256);
    assert(lowRow > highRow + 40, 'Equal noise density has higher integrated voltage in the RF band');
    assert(views.pixels.some(value => value !== views.palette[0]));
    selector.value = 'spectrogram'; selector.dispatchEvent(new h.w.Event('change'));
    assert.equal(h.d.getElementById('history-level-unit').textContent, 'dBV');
    assert(views.orderedPixels.some(value => value !== views.palette[0]));
    const range = {from: 1000, to: 10000};
    const column = views.columns(range, 1)[0];
    const frequencies = plot.frameStatus.spectrum.frequencies;
    assert(frequencies[column.first] >= 1000 && frequencies[column.last] <= 10000);
});
