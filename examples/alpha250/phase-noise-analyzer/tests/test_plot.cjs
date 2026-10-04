const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');

function fixture(t) {
    const dom = new JSDOM('<span id="refresh-rate">— FPS</span><div id="plot-empty"></div><table id="decade-values-table"></table><input id="show-smoothed-trace" type="checkbox" checked><button id="capture-reference" disabled></button><button id="clear-reference" disabled></button><div id="reference-info" hidden><span id="reference-status"></span></div>', {runScripts: 'outside-only', pretendToBeVisual: true});
    const w = dom.window;
    t.after(() => w.close());
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../web/plot.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.Plot = Plot;');
    // Select methods without starting the constructor's polling loop.
    const plot = Object.create(w.Plot.prototype);
    const state = {range: null, redraw: null, fits: 0, legendRefreshes: 0};
    plot.document = w.document;
    plot.rateStarted = 0; plot.displayedFrames = 0; plot.lastTableUpdate = -Infinity;
    w.setTimeout = () => 0;
    plot.n_pts = 16385; plot.samplingFrequency = 5e6;
    plot.plot_data = []; plot.laserPlotType = 'phase'; plot.yLabel = 'Phase noise (dBc/Hz)';
    plot.plotBasics = {
        setRangeX(low, high) { state.range = [low, high]; },
        setLinY() { state.fits++; },
        refreshLegend() { state.legendRefreshes++; },
        redraw(data, size, peak, label, callback, reference, final, traces) { state.redraw = {data, size, reference, smooth: traces?.[0]?.data}; }
    };
    plot.driver = {
        parameters: {data_size: 16385, fs: 5e6, channel: 0, cic_rate: 20, fft_navg: 8, analyzer_mode: 'rf', fdds0: 10e6, fdds1: 20e6, interferometer_delay: 1e-9},
        async getPhaseNoise() { return new Float32Array(16385).fill(2); }
    };
    plot.decadeValuesTable = w.document.querySelector('table');
    plot.showSmoothedInput = w.document.querySelector('input');
    w.app = {dds: {async getDDSFreq() { return 10e6; }}};
    w.requestAnimationFrame = () => 0;
    plot._busy = false; plot._targetHz = 60; plot._lastTick = -1000;
    plot.setFreqAxis();
    return {plot, state, window: w};
}

test('real FFT axis includes DC, exact bin centers and Nyquist', t => {
    const {plot, state} = fixture(t);
    plot.setFreqAxis();
    assert.equal(plot.plot_data[0][0], 0);
    assert.equal(plot.plot_data[64][0], 64 * 5e6 / 32768);
    assert.equal(plot.plot_data.at(-1)[0], 2.5e6);
    assert.deepEqual(state.range, [2 * 5e6 / 32768, .75 * 2.5e6]);
});

test('decade table averages linear density around the reported offsets', t => {
    const {plot} = fixture(t);
    plot.setFreqAxis();
    for (let i = 0; i < plot.n_pts; i++) plot.linear_plot_data[i][1] = i >= 2 ? (i % 2 ? 1 : 10) : 1e20;
    const scale = Math.pow(10, .05);
    for (const [frequency, value] of plot.getDecadeValues()) {
        const rows = plot.linear_plot_data.filter(([f], i) => i >= 2 && f >= frequency / scale && f <= frequency * scale);
        const expected = 10 * Math.log10(rows.reduce((sum, row) => sum + row[1], 0) / rows.length);
        if (rows.length) assert.ok(Math.abs(value - expected) < 1e-10);
        else assert.ok(Number.isNaN(value));
    }
});

test('received spectrum length controls the axis and preserves density units', async t => {
    const {plot, state} = fixture(t);
    plot.n_pts = 16384; // old cached parameters must not drop the Nyquist sample
    await plot.updatePlot();
    assert.equal(plot.n_pts, 16385);
    assert.equal(state.redraw.size, 16385);
    assert.equal(state.redraw.data.at(-1)[0], 2.5e6);
    assert.ok(Number.isNaN(state.redraw.data[0][1]));
    assert.ok(Number.isNaN(state.redraw.data[1][1]));
    assert.ok(Math.abs(state.redraw.data[64][1]) < 1e-12); // 2 rad²/Hz -> 0 dBc/Hz
    assert.ok(Math.abs(state.redraw.smooth[64][1]) < 1e-12);
    plot.laserPlotType = 'frequency'; plot._busy = false; plot._lastTick = -1000;
    await plot.updatePlot();
    assert.ok(Math.abs(state.redraw.data[64][1] - 10 * Math.log10(2 * (64 * 5e6 / 32768) ** 2)) < 1e-10);
});

test('smoothing averages linear density and excludes DC, invalid bins and negative values', t => {
    const {plot} = fixture(t);
    plot.n_pts = 1025; plot.setFreqAxis();
    const density = new Float32Array(plot.n_pts);
    for (let i = 0; i < density.length; i++) density[i] = (i % 2 ? 2 : 20) + i / 1000;
    density[0] = density[1] = 1e20;
    density[61] = NaN; density[62] = -1; density[63] = Infinity;
    const raw = density.slice();
    plot.computeDisplaySpectrum(density, 2);
    plot.computeSmoothedPlot(2);
    const scale = Math.pow(10, .05);
    for (const bin of [2, 64, 200, 1024]) {
        const selected = [...raw].map((value, i) => ({value, i})).filter(({value, i}) =>
            i >= 2 && i >= bin / scale && i <= bin * scale && Number.isFinite(value) && value >= 0);
        const mean = selected.reduce((sum, {value}) => sum + value / 2, 0) / selected.length;
        assert.ok(Math.abs(plot.smooth_plot_data[bin][1] - 10 * Math.log10(mean)) < 1e-10);
    }
    assert.ok(Number.isNaN(plot.smooth_plot_data[1][1]));
    assert.deepEqual(density, raw);
});

test('smoothing visibility changes only the overlay; raw PSD stays exportable', async t => {
    const {plot, state} = fixture(t);
    plot.showSmoothedInput.checked = false;
    await plot.updatePlot();
    assert.equal(state.redraw.smooth, undefined);
    assert.equal(plot.phase_psd.length, 16385);
    assert.equal(plot.phase_psd[64], 2);
    assert.ok(Math.abs(plot.smooth_plot_data[64][1]) < 1e-12);
});

test('CSV exports raw and smoothed display values plus the original linear PSD', async t => {
    const {plot, window: w} = fixture(t);
    await plot.updatePlot();
    w.document.body.insertAdjacentHTML('beforeend', `
        <input data-command="setReferenceClock" type="radio" checked data-valuestr="internal">
        <input name="channel" type="radio" value="0" checked>
        <span><input class="dds-input" value="10000000.637"><select class="lo-unit"><option>Hz</option></select></span>
        <input class="cic-rate-input" value="20"><input class="plot-navg-input" value="1">
        <span><button class="export-data">CSV</button><a></a></span>`);
    let exported, blob, filename;
    w.Blob = Blob;
    w.URL.createObjectURL = value => { blob = value; return 'blob:http://test/download'; };
    w.URL.revokeObjectURL = () => {};
    w.HTMLAnchorElement.prototype.click = function () { filename = this.download; };
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../web/export-file/export-file.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.ExportFile = ExportFile;');
    new w.ExportFile(w.document, plot);
    w.document.querySelector('.export-data').click();
    exported = await blob.text();
    assert.ok(exported.includes('"LO 0 frequency (Hz)",10000000'));
    assert.match(filename, /^phase-noise-analyzer-.*\.csv$/);
    assert.ok(exported.includes('"Phase PSD (rad^2/Hz)"'));
    const table = exported.split('"Offset frequency (Hz)"')[1].trim().split('\n').slice(1);
    assert.equal(table.length, 16385);
    const row = table[64].split(',').map(Number);
    assert.equal(row[0], 64 * 5e6 / 32768);
    assert.ok(Math.abs(row[1]) < 1e-12 && Math.abs(row[2]) < 1e-12);
    assert.equal(row[3], 2);

    // The same CSV workflow exports a frozen reference on its own grid.
    plot.captureReference();
    plot.driver.parameters.fs = 1e6;
    plot.driver.parameters.channel = 1;
    plot._busy = false; plot._lastTick = -1000;
    await plot.updatePlot();
    w.document.querySelector('.export-data').click();
    exported = await blob.text();
    const reference = exported.split('Reference trace\n')[1];
    assert.ok(reference.includes('"Input channel",0\n'));
    assert.ok(reference.includes('"Sampling frequency (Hz)",5000000\n'));
    const referenceTable = reference.split('"Offset frequency (Hz)"')[1].trim().split('\n').slice(1);
    assert.equal(referenceTable.length, 16385);
    const referenceRow = referenceTable[64].split(',').map(Number);
    assert.equal(referenceRow[0], 64 * 5e6 / 32768);
    assert.equal(referenceRow[3], 2);
    // Red Pitaya has a fixed clock and no reference-clock radio group.
    w.document.body.dataset.board = 'red-pitaya';
    w.document.querySelector('[data-command="setReferenceClock"]').remove();
    w.document.querySelector('.export-data').click();
    exported = await blob.text();
    assert.ok(exported.includes('"Reference clock",Fixed onboard'));
});


test('FFT-style reference capture copies the full PSD and frame settings; replacement and clear work', async t => {
    const {plot, state, window: w} = fixture(t);
    await plot.updatePlot();
    const fits = state.fits;
    plot.captureReference();
    assert.equal(state.fits, fits, 'capture preserves the Y zoom');
    assert.equal(plot.referencePSD.length, 16385);
    assert.equal(plot.referencePSD[64], 2);
    assert.equal(w.document.getElementById('capture-reference').textContent, 'Replace ref');
    assert.equal(w.document.getElementById('clear-reference').disabled, false);
    assert.equal(w.document.getElementById('reference-info').hidden, false);
    plot.phase_psd.fill(20);
    plot.driver.parameters.fs = 1e6;
    plot.driver.parameters.channel = 1;
    assert.equal(plot.referencePSD[64], 2);
    assert.equal(plot.referenceParameters.channel, 0);
    plot._busy = false; plot._lastTick = -1000;
    await plot.updatePlot();
    assert.equal(state.redraw.reference[64][0], 64 * 5e6 / 32768);
    assert.equal(state.redraw.data[64][0], 64 * 1e6 / 32768);
    plot.laserPlotType = 'frequency'; plot.showSmoothedInput.checked = false;
    plot._busy = false; plot._lastTick = -1000;
    await plot.updatePlot();
    assert.equal(state.redraw.reference[64][1], 10 * Math.log10(2 * (64 * 5e6 / 32768) ** 2));
    plot.captureReference();
    assert.equal(plot.referenceParameters.channel, 1);
    assert.equal(plot.referenceParameters.fs, 1e6);
    const fitsBeforeClear = state.fits;
    plot.clearReference();
    assert.equal(state.fits, fitsBeforeClear, 'clear preserves the Y zoom');
    assert.equal(plot.referencePSD, undefined);
    assert.equal(plot.reference_data, undefined);
    assert.equal(w.document.getElementById('capture-reference').textContent, 'Capture ref');
    assert.equal(w.document.getElementById('clear-reference').disabled, true);
    assert.equal(w.document.getElementById('reference-info').hidden, true);
});

test('settling, failed acquisition and unset LO disable capture without clearing the reference', async t => {
    const {plot, state, window: w} = fixture(t);
    await plot.updatePlot(); plot.captureReference();
    const psd = plot.referencePSD;
    plot.driver.getPhaseNoise = async () => new Float32Array(16385);
    plot._busy = false; plot._lastTick = -1000;
    await plot.updatePlot();
    assert.equal(w.document.getElementById('capture-reference').disabled, true);
    plot.captureReference(); assert.equal(plot.referencePSD, psd);
    plot.driver.parameters.fdds0 = 0;
    plot._busy = false; plot._lastTick = -1000;
    await plot.updatePlot();
    assert.equal(w.document.getElementById('capture-reference').disabled, true);
    assert.equal(plot.referencePSD, psd);
    plot.driver.parameters.fdds0 = 10e6;
    plot.driver.getPhaseNoise = async () => { throw new Error('Acquisition failed'); };
    w.console.error = () => {};
    plot._busy = false; plot._lastTick = -1000;
    await plot.updatePlot();
    assert.equal(w.document.getElementById('capture-reference').disabled, true);
    assert.equal(plot.referencePSD, psd);
    plot.clearReference();
    assert.equal(state.redraw.reference, undefined);
});

test('web driver strips the vector byte-length prefix and preserves DC through Nyquist', async t => {
    const {window: w} = fixture(t);
    for (const [file, exports] of [
        ['../../../../web/koheron.ts', ['Client']],
        ['../web/phase-noise-analyzer.ts', ['PhaseNoiseAnalyzer']]
    ]) {
        w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, file), 'utf8'),
            {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + exports.map(name => `\nwindow.${name} = ${name};`).join(''));
    }
    const client = Object.create(w.Client.prototype);
    const wire = new w.DataView(new w.ArrayBuffer(12 + 4 * 16385));
    wire.setUint16(4, 5); wire.setUint16(6, 7); wire.setUint32(8, 4 * 16385);
    for (let bin = 0; bin < 16385; bin++) wire.setFloat32(12 + 4 * bin, bin + .5, true);
    client.getDriver = () => ({id: 5, getCmds: () => ({get_phase_noise: {id: 7, args: []}})});
    client._readBaseAsync = async mode => client.getPayload(mode, {data: wire.buffer}).dv;
    const psd = await new w.PhaseNoiseAnalyzer(client).getPhaseNoise();
    assert.equal(psd.length, 16385);
    assert.equal(psd[0], .5);
    assert.equal(psd[64], 64.5);
    assert.equal(psd[16384], 16384.5);
});

test('exports wait for a valid frame and become unavailable again after a read failure', async t => {
    const {plot, window: w} = fixture(t);
    w.document.body.insertAdjacentHTML('beforeend', '<button class="export-data"></button><button class="export-plot"></button>');
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../web/export-file/export-file.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.ExportFile = ExportFile;');
    let downloads = 0;
    w.URL.createObjectURL = () => { downloads++; return 'blob:http://test/download'; };
    const exporter = new w.ExportFile(w.document, plot);
    exporter.exportData(); exporter.exportPlot();
    assert.equal(downloads, 0);
    assert.equal(w.document.querySelector('.export-plot').disabled, true);
    await plot.updatePlot();
    assert.equal(w.document.querySelector('.export-data').disabled, false);
    assert.equal(w.document.querySelector('.export-plot').disabled, false);
    const failures = [];
    plot.onConnectionError = error => failures.push(error.message);
    plot.driver.getPhaseNoise = async () => { throw new Error('Disconnected'); };
    plot._busy = false; plot._lastTick = -1000;
    w.console.error = () => {};
    await plot.updatePlot();
    assert.deepEqual(failures, ['Disconnected']);
    assert.equal(plot.frameStatus, undefined);
    assert.equal(w.document.querySelector('.export-data').disabled, true);
    exporter.exportData(); exporter.exportPlot();
    assert.equal(downloads, 0);
});

test('PNG includes a white background, measurement units and every trace label at HiDPI resolution', async t => {
    const {plot, window: w} = fixture(t);
    await plot.updatePlot();
    w.document.body.insertAdjacentHTML('beforeend', '<div id="plot-placeholder"><canvas class="flot-base"></canvas></div><button class="export-plot"></button>');
    const canvas = w.document.querySelector('canvas');
    canvas.width = 640; canvas.height = 400;
    Object.defineProperty(canvas, 'clientWidth', {value: 320});
    const draw = [];
    let image;
    w.HTMLCanvasElement.prototype.getContext = function () {
        image = this;
        return {measureText: text => ({width: text.length * 6}), scale: (...args) => draw.push(['scale', ...args]),
            fillRect: (...args) => draw.push(['fillRect', ...args]), fillText: (...args) => draw.push(['text', ...args]),
            drawImage: (...args) => draw.push(['image', ...args])};
    };
    w.Blob = Blob;
    w.HTMLCanvasElement.prototype.toBlob = function (callback) { callback(new Blob([], {type: 'image/png'})); };
    w.URL.createObjectURL = () => 'blob:http://test/download';
    w.URL.revokeObjectURL = () => {};
    w.HTMLAnchorElement.prototype.click = () => {};
    plot.plotBasics.plot = {getData: () => [{label: 'Raw', color: 'blue'}, {label: 'Smoothed', color: 'green'}]};
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../web/export-file/export-file.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.ExportFile = ExportFile;');
    new w.ExportFile(w.document, plot);
    w.document.querySelector('.export-plot').click();
    assert.equal(image.width, 640);
    assert.ok(image.height > 400);
    assert.deepEqual(draw.find(c => c[0] === 'scale'), ['scale', 2, 2]);
    assert.equal(draw.find(c => c[0] === 'fillRect')[3], 320);
    assert.ok(draw.some(c => c[0] === 'text' && c[1].includes('dBc/Hz')));
    assert.ok(draw.some(c => c[0] === 'text' && c[1] === 'Offset frequency (Hz)'));
    assert.ok(draw.some(c => c[0] === 'text' && c[1] === 'Raw'));
    assert.ok(draw.some(c => c[0] === 'text' && c[1] === 'Smoothed'));
    assert.equal(draw.find(c => c[0] === 'image')[1], canvas);
});

test('a cursor clicked on the smoothed trace stays with that trace across live redraws', t => {
    const {window: w} = fixture(t);
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../../../../web/plot-basics/plot-basics.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PlotBasics = PlotBasics;');
    const basics = Object.create(w.PlotBasics.prototype);
    const events = new Map();
    let prefix, highlighted;
    Object.assign(basics, {
        plot_placeholder: {bind: (name, listener) => events.set(name, listener)},
        plot: {getData: () => basics.seriesOne, setData() {}, draw() {}, unhighlight() {},
            highlight(series) { highlighted = series.label; }},
        options: {legend: {}}, seriesOne: [{label: 'Raw'}], reset_range: false, decimate: false,
        clickDatapoint: [], clickDatapointSpan: {style: {}}, peakDatapointSpan: {style: {}},
        range_x: {from: 0, to: 30}, range_y: {from: 0, to: 100},
        updateDatapointSpan(point, span, label) { prefix = label; }
    });
    basics.showClickPoint();
    events.get('plotclick')({}, {}, {datapoint: [15, 30], series: {label: 'Smoothed'}});
    assert.equal(prefix, 'Smoothed ');
    const smooth = [{label: 'Smoothed', data: [[0, 20], [30, 60]]}];
    basics.redraw([[0, 90], [30, 10]], 2, [], 'PSD', () => {}, [[0, 10], [30, 40]], false, smooth);
    assert.equal(basics.clickDatapoint[1], 40);
    assert.equal(highlighted, 'Smoothed');
    basics.redraw([[0, 90], [30, 10]], 2, [], 'PSD', () => {}, undefined, false, smooth);
    assert.equal(basics.clickDatapoint[1], 40);
    assert.equal(highlighted, 'Smoothed');
    basics.redraw([[0, 90], [30, 10]], 2, [], 'PSD', () => {});
    assert.equal(basics.clickDatapoint.length, 0);
});

test('logarithmic zoom retains distinct frequency labels between decade marks', t => {
    const {window: w} = fixture(t);
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../../../../web/plot-basics/plot-basics.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PlotBasics = PlotBasics;');
    const basics = Object.create(w.PlotBasics.prototype);
    basics.options = {xaxis: {}, grid: {}};
    basics.setLogX(true);
    for (const axis of [{min: 1200, max: 9400}, {min: 1200, max: 1700}, {min: 2000, max: 2001}]) {
        const ticks = basics.options.xaxis.ticks(axis);
        const labels = ticks.map(v => basics.options.xaxis.tickFormatter(v, axis));
        assert.ok(ticks.length >= 2);
        assert.equal(new Set(labels).size, ticks.length);
        assert.ok(ticks.every(v => v >= axis.min && v <= axis.max));
    }
});

test('analyzer can retain its page on socket loss while other SDK clients retain automatic reload', t => {
    const {window: w} = fixture(t);
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../../../../web/koheron.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.WebSocketPool = WebSocketPool;');
    let socket;
    const timers = [];
    w.setTimeout = (fn, ms) => { timers.push(ms); return 1; };
    w.WebSocketPool.prototype._newWebSocket = () => socket = {
        readyState: 1,
        close() { this.readyState = 3; this.onclose?.(); }
    };
    w.WebSocketPool.prototype.waitForConnection = (s, interval, done) => done();
    const errors = [];
    let pool = new w.WebSocketPool(1, 'ws://test', () => {}, error => { errors.push(error.message); pool.exit(); });
    socket.onopen(); socket.onclose();
    assert.deepEqual(errors, ['WebSocket connection lost']);
    assert.equal(timers.length, 0);
    socket.onclose();
    assert.equal(errors.length, 1, 'intentional socket cleanup does not report another loss');
    pool = new w.WebSocketPool(1, 'ws://test', () => {});
    socket.onopen(); socket.onclose();
    assert.deepEqual(timers, [1000]);
    pool.exit();
});


test('plot defaults to a 60 FPS target and disposal cancels the pending update', t => {
    const {plot, window: w} = fixture(t);
    Object.assign(plot.plotBasics, {setLogX() {}, enableDecimation() {}, setPrimaryTraceLabel() {}});
    const live = new w.Plot(w.document, plot.driver, plot.plotBasics);
    assert.equal(live._targetHz, 60);
    const cancelled = [];
    w.clearTimeout = id => cancelled.push(id);
    live.timer = 42;
    live.dispose();
    assert(cancelled.includes(42));
    assert.equal(w.document.getElementById('refresh-rate').textContent, '— FPS');
});

test('FPS counts completed valid spectrum displays; reference redraws and unavailable data do not count', async t => {
    const {plot, window: w} = fixture(t);
    let now = 1000;
    Object.defineProperty(w.performance, 'now', {value: () => now});
    plot.rateStarted = 0; plot.displayedFrames = 59;
    plot.plotBasics.redraw = (data, size, peak, label, done) => done();
    await plot.updatePlot();
    assert.equal(w.document.getElementById('refresh-rate').textContent, '60 FPS');
    assert.equal(plot.displayedFrames, 0);
    plot.captureReference();
    assert.equal(plot.displayedFrames, 0, 'reference redraw is not a new spectrum');
    now += 1000;
    plot.driver.getPhaseNoise = async () => new Float32Array(16385);
    await plot.updatePlot();
    assert.equal(w.document.getElementById('refresh-rate').textContent, '— FPS');
    assert.equal(plot.displayedFrames, 0);
});

test('polling respects the target, permits only one in-flight read, and pauses while hidden', async t => {
    const {plot, window: w} = fixture(t);
    const timers = [];
    w.setTimeout = (callback, delay) => { timers.push({callback, delay}); return timers.length; };
    let now = 1000;
    Object.defineProperty(w.performance, 'now', {value: () => now});
    plot._lastTick = now - 5;
    await plot.updatePlot();
    assert.ok(Math.abs(timers[0].delay - (1000 / 60 - 5)) < 1e-9);
    let finish, reads = 0;
    plot.driver.getPhaseNoise = () => { reads++; return new Promise(resolve => { finish = resolve; }); };
    now += 20;
    const pending = plot.updatePlot();
    await Promise.resolve();
    await plot.updatePlot();
    assert.equal(reads, 1);
    Object.defineProperty(w.document, 'hidden', {value: true, configurable: true});
    plot.plotBasics.redraw = (data, size, peak, label, done) => done();
    finish(new Float32Array(16385).fill(2));
    await pending;
    assert.equal(timers.length, 1, 'hidden page must not schedule another read');
    await plot.updatePlot();
    assert.equal(reads, 1);
});
