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
    const state = {writes: [], reads: [], exits: 0, window: 1, channel: 0, operation: 0, ranges: [0, 0], reference: 2, dac: [.1, .2, .3, .4], errors: [], generations: {FFT: 1, Decimator: 1}, sequences: [1, 1, 1], noise: 1e-16};
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
                'web/temperature-sensor', 'web/fft', 'web/fft/controls', 'web/fft/plot', 'web/fft/export-file', 'web/plot-basics'];
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
        send(command) {
            state.writes.push(command);
            if (command.driver === 'PrecisionDac' && command.name === 'set_dac_value_volts') {
                state.dac[command.args[0]] = command.args[1];
            }
        }
        async readUint32(command) {
            state.reads.push(command);
            switch (command.name) {
                case 'restart_acquisition': state.sequences = [1, 1, 1]; return ++state.generations[command.driver];
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
                case 'get_dac_values': return new w.Float32Array(state.dac);
                case 'get_temperatures': return new w.Float32Array([30, 40, 50]);
                case 'get_supplies_ui': return new w.Float32Array([.1, 12, .02, 3.3]);
                default: throw new Error(command.name);
            }
        }
        async readTupleWithFloat32Vector(command, format, bytes) {
            state.reads.push(command);
            assert.equal(format, 'IQ'); assert.equal(bytes, 12);
            const band = command.name === 'get_spectrum_snapshot1' ? 0 : command.name === 'get_spectrum_snapshot0' ? 1 : 2;
            const values = new w.Float32Array(band === 2 ? 4096 : 4097).fill(state.noise);
            if (band === 2) values[1311] = 1e-8;
            return {metadata: [state.generations[command.driver], state.sequences[band]], values};
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
    const files = ['web/precision-channels/precision-dac.ts', 'web/fft/driver.ts', 'web/fft/controls/fft-app.ts', 'web/precision-channels/precision-channels-app.ts',
        'web/inputs/digit-input.ts', 'web/fft/plot/spectrum-history.ts', 'web/fft/plot/spectrum-views.ts',
        'web/fft/plot/plot.ts', 'web/fft/export-file/export-file.ts', 'web/fft/workspace.ts',
        ...['adc-range/ltc2387.ts', 'clock-generator/clock-generator.ts',
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

test('Alpha15 shared DAC edits preserve acquisition and references while telemetry retains drafts', async t => {
    const h = await host(t), plot = h.app.plot;
    plot.captureReference();
    const reference = plot.referenceStatus, epoch = plot.history.epoch;
    const restarts = h.state.reads.filter(c => c.name === 'restart_acquisition').length;
    const input = h.d.querySelector(".precision-dac-input[data-channel='3']");
    input.value = '123.456 mV';
    input.dispatchEvent(new h.w.Event('input', {bubbles: true}));
    await h.app.board.poll();
    assert.equal(input.value, '123.456 mV', 'Board telemetry must preserve a pending DAC edit');
    assert.equal(h.state.writes.length, 0);
    const pendingTimers = new Set(h.timers.keys());
    input.dispatchEvent(new h.w.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    // Advance the newly queued edit; the fixture freezes background poll timers.
    for (const [id, callback] of h.timers) {
        if (!pendingTimers.has(id)) { h.timers.delete(id); callback(); }
    }
    await h.flush();
    assert.deepEqual(h.state.writes.map(c => [c.driver, c.name, ...c.args]), [
        ['PrecisionDac', 'set_dac_value_volts', 3, .123456]
    ]);
    assert.equal(input.value, '123.456');
    assert.equal(plot.history.epoch, epoch);
    assert.strictEqual(plot.referenceStatus, reference);
    assert.equal(h.state.reads.filter(c => c.name === 'restart_acquisition').length, restarts);
    assert.deepEqual(Array.from(h.d.querySelectorAll('.temperature-span'), node => node.textContent), ['30.0', '40.0', '50.0']);
    assert.deepEqual(Array.from(h.d.querySelectorAll('.supply-span'), node => node.textContent), ['12.000', '3.300', '100.0', '20.0']);
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
    const lowReads = h.state.reads.filter(c => c.name === 'get_spectrum_snapshot1').length;
    await fft.read_psd();
    assert.equal(h.state.reads.filter(c => c.name === 'get_spectrum_snapshot1').length, lowReads);
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

test('late spectrum replies cannot restore cache entries invalidated by a settings change', async t => {
    const h = await host(t), fft = h.app.fft;
    h.app.plot.setPaused(true);
    const client = fft.client, original = client.readTupleWithFloat32Vector.bind(client);
    let release, started;
    const reading = new Promise(resolve => { started = resolve; });
    client.readTupleWithFloat32Vector = command => {
        if (command.name !== 'get_spectrum_snapshot1') return original(command, 'IQ', 12);
        started();
        return new Promise(resolve => { release = resolve; });
    };
    fft.snapshots = [];
    fft.fetchedAt = [-Infinity, -Infinity, -Infinity];
    const oldRead = fft.readSpectrum();
    await reading;
    h.state.channel = 1;
    await fft.getControlParameters();
    h.state.noise = 1e-12;
    client.readTupleWithFloat32Vector = original;
    const fresh = await fft.readSpectrum();
    release({metadata: [h.state.generations.Decimator - 1, 1], values: new h.w.Float32Array(4097).fill(1e-16)});
    assert.equal(await oldRead, undefined);
    assert.equal(fresh.status.channel, 1);
    assert(Math.abs(fresh.psd[1] - 1e-12) < 1e-19);
    assert(Math.abs(fft.snapshots[0][1] - 1e-12) < 1e-19);
    assert(Math.abs(fft.snapshots[1][1] - 1e-12) < 1e-19);
});

test('local edits suspend spectrum reads until fresh controls arrive, ignoring a pre-edit control reply', async t => {
    const h = await host(t), fft = h.app.fft;
    h.app.plot.setPaused(true);
    const client = fft.client, original = client.readTuple.bind(client);
    let release, started;
    const reading = new Promise(resolve => { started = resolve; });
    client.readTuple = async (command, format) => {
        const value = await original(command, format);
        started();
        return new Promise(resolve => { release = () => resolve(value); });
    };
    const oldControls = fft.getControlParameters();
    await reading;
    fft.setInputChannel(1);
    h.state.channel = 1;
    const before = h.state.reads.length;
    assert.equal(await fft.readSpectrum(), undefined);
    assert.equal(h.state.reads.length, before);
    release(); await oldControls;
    assert.equal(fft.status.channel, 0);
    assert.equal(await fft.readSpectrum(), undefined);
    client.readTuple = original;
    await fft.getControlParameters();
    assert.equal((await fft.readSpectrum()).status.channel, 1);
    for (const selector of [".adc-range[value='1']", ".clkgen-input[value='0']"]) {
        h.d.querySelector(selector).dispatchEvent(new h.w.Event('change'));
        assert.equal(await fft.readSpectrum(), undefined);
        await fft.getControlParameters();
        assert(await fft.readSpectrum());
    }
});

test('a malformed band is not cached and the next read retries that band', async t => {
    const h = await host(t), fft = h.app.fft;
    h.app.plot.setPaused(true);
    fft.snapshots = [];
    fft.fetchedAt = [-Infinity, -Infinity, -Infinity];
    const client = fft.client, original = client.readTupleWithFloat32Vector.bind(client);
    h.state.sequences = [2, 2, 2];
    client.readTupleWithFloat32Vector = async command => {
        const reply = await original(command, 'IQ', 12);
        if (command.name === 'get_spectrum_snapshot1') reply.values = new h.w.Float32Array(1);
        return reply;
    };
    await assert.rejects(fft.readSpectrum(), /Incomplete spectrum band/);
    assert.equal(fft.snapshots[0], undefined);
    client.readTupleWithFloat32Vector = original;
    assert(await fft.readSpectrum());
});

test('the plot retains the settings attached to a returned spectrum frame', async t => {
    const h = await host(t), fft = h.app.fft, plot = h.app.plot;
    plot.setPaused(true);
    h.state.sequences = [2, 2, 2]; fft.fetchedAt = [-Infinity, -Infinity, -Infinity];
    const frame = await fft.readSpectrum();
    fft.status = {...fft.status, channel: 1};
    fft.readSpectrum = async () => frame;
    plot.setPaused(false);
    await h.flush();
    assert.equal(plot.frameStatus.channel, 0);
    assert.equal(plot.history.status.channel, 0);
});

test('history receives each stitched acquisition once, only after every band advances', async t => {
    const h = await host(t), fft = h.app.fft, plot = h.app.plot;
    plot.setPaused(true);
    const samples = plot.history.samples;
    const poll = async () => {
        fft.fetchedAt = [-Infinity, -Infinity, -Infinity];
        const frame = await fft.readSpectrum();
        if (frame) plot.acceptSpectrum(frame.psd, 1, frame.status);
        return frame;
    };
    assert.equal(await poll(), undefined);
    h.state.sequences[2] = 50;
    assert.equal(await poll(), undefined);
    h.state.sequences[1] = 5;
    assert.equal(await poll(), undefined);
    assert.equal(plot.history.samples, samples);
    h.state.sequences[0] = 2;
    assert(await poll());
    assert.equal(plot.history.samples, samples + 1);
    assert.equal(await poll(), undefined);
    assert.equal(plot.history.samples, samples + 1);
});

test('restart waits for complete fresh averages and rejects the previous acquisition generation', async t => {
    const h = await host(t), fft = h.app.fft;
    h.app.plot.setPaused(true);
    fft.setInputChannel(1); h.state.channel = 1;
    await fft.getControlParameters();
    h.state.sequences = [0, 0, 0];
    assert.equal(await fft.readSpectrum(), undefined);
    const lowReads = h.state.reads.filter(c => c.name === 'get_spectrum_snapshot1').length;
    assert.equal(await fft.readSpectrum(), undefined);
    assert.equal(h.state.reads.filter(c => c.name === 'get_spectrum_snapshot1').length, lowReads);
    assert.equal(fft.waitingForSpectrum, true);
    h.app.plot.setPaused(false); await h.flush();
    assert.equal(h.d.getElementById('connection-status').textContent, 'Waiting for fresh spectrum…');
    h.app.plot.setPaused(true);
    assert(fft.snapshots.every(values => !values));
    h.state.sequences = [1, 1, 1];
    const client = fft.client, original = client.readTupleWithFloat32Vector.bind(client);
    client.readTupleWithFloat32Vector = async command => {
        const reply = await original(command, 'IQ', 12);
        if (command.name === 'get_spectrum_snapshot1') reply.metadata[0]--;
        return reply;
    };
    fft.fetchedAt = [-Infinity, -Infinity, -Infinity];
    assert.equal(await fft.readSpectrum(), undefined);
    assert.equal(fft.snapshots[0], undefined);
    client.readTupleWithFloat32Vector = original;
    fft.fetchedAt = [-Infinity, -Infinity, -Infinity];
    const frame = await fft.readSpectrum();
    assert(frame); assert.equal(frame.status.channel, 1);
    assert.equal(fft.waitingForSpectrum, false);
});

test('the actual client decodes generation and sequence with an owned spectrum vector', async () => {
    const vm = require('node:vm');
    const context = vm.createContext({assert});
    vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(root, 'web/koheron.ts'), 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText, context);
    await vm.runInContext(`(async () => {
        // A sliced network buffer also exercises unaligned payload ownership.
        const wire = new DataView(new ArrayBuffer(27), 3, 24);
        wire.setUint32(0, 7);
        wire.setBigUint64(4, 4294967301n);
        wire.setUint32(12, 8);
        wire.setFloat32(16, 1e-12, true);
        wire.setFloat32(20, NaN, true);
        const client = {_readBaseAsync: async () => wire, deserialize: Client.prototype.deserialize};
        const read = () => Client.prototype.readTupleWithFloat32Vector.call(client, {}, 'IQ', 12);
        const reply = await read();
        assert.equal(reply.metadata[0], 7); assert.equal(reply.metadata[1], 4294967301);
        assert.equal(reply.values.length, 2); assert(Number.isNaN(reply.values[1]));
        assert(Math.abs(reply.values[0] - 1e-12) < 1e-19);
        reply.values[0] = 99;
        assert(Math.abs(wire.getFloat32(16, true) - 1e-12) < 1e-19);
        wire.setUint32(12, 7);
        await assert.rejects(read(), /Invalid spectrum vector length/);
    })()`, context);
});

test('a restart by another client refreshes controls and resumes without repeated server restarts', async t => {
    const h = await host(t), fft = h.app.fft;
    h.app.plot.setPaused(true);
    h.state.generations.FFT++; h.state.generations.Decimator++;
    h.state.sequences = [1, 1, 1];
    const restarts = h.state.reads.filter(c => c.name === 'restart_acquisition').length;
    let frame;
    for (let i = 0; i < 4; i++) {
        fft.fetchedAt = [-Infinity, -Infinity, -Infinity];
        frame = await fft.readSpectrum();
        if (frame) break;
        assert.equal(fft.controlsPending, true);
        await fft.getControlParameters();
    }
    assert(frame);
    assert.deepEqual(Array.from(fft.generations), [h.state.generations.Decimator, h.state.generations.Decimator, h.state.generations.FFT]);
    assert.equal(h.state.reads.filter(c => c.name === 'restart_acquisition').length, restarts);
});
