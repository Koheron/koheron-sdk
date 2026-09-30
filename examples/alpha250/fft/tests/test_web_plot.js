// Run with NODE_PATH pointing to the SDK node_modules if using an isolated worktree.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const root = path.resolve(__dirname, '../../../..');
const context = vm.createContext({console, assert, performance});
for (const file of ['examples/alpha250/fft/web/plot/plot.ts', 'web/plot-basics/plot-basics.ts']) {
    vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText, context);
}
vm.runInContext(`
(async () => {
    const fields = new Map();
    let changeUnit;
    const unit = {value: 'dBm-Hz', addEventListener(event, cb) { changeUnit = cb; }, disabled: false};
    const doc = {
        getElementById(id) {
            if (!fields.has(id)) fields.set(id, {dataset: {}, textContent: '', addEventListener() {}});
            return fields.get(id);
        },
        querySelector: selector => selector === '.peak-input' ? {addEventListener() {}} : unit,
        querySelectorAll: selector => selector === '.unit-input' ? [unit] : []
    };
    globalThis.$ = () => ({on() {}, off() {}});
    globalThis.window = {setTimeout: () => 1, clearTimeout() {}, requestAnimationFrame: () => 1, cancelAnimationFrame() {}};
    globalThis.setTimeout = window.setTimeout;
    let reads = 0;
    const psd = new Float32Array(4096).fill(1e-16);
    psd[0] = 0; // Zero DC must not prevent finding a finite peak.
    psd[1311] = 1e-8;
    const fft = {fft_size: 8192, status: {fs: 250e6, W1: .25, W2: .375, dds_freq: [40e6, 0]},
                 async read_psd() { reads++; return psd; }};
    let drawn;
    const basics = {enableSpectrumReduction() {}, setLinY() {}, setRangeX() {}, getRangeX() { return {from: 0, to: 125}; },
                    redraw(data, count, peak, label, callback) { drawn = {data, count, peak}; callback(); }};
    const plot = new Plot(doc, fft, basics);
    await Promise.resolve();
    assert.equal(drawn.count, 4096);
    assert.equal(drawn.data[0][0], 0);
    assert.equal(drawn.data[4095][0], 124.969482421875);
    assert.equal(drawn.peak[0], 40.008544921875);
    assert.equal(fields.get('peak-frequency').textContent, '40.008545 MHz');
    plot.setPaused(true);
    await plot.updatePlot();
    assert.equal(reads, 1);
    assert.equal(unit.disabled, false);
    const retainedPSD = psd[1311];
    psd[1311] = 1e-5; // The client may reuse its response buffer.
    fft.status.fs = 200e6;
    fft.status.W2 = 1; // Hardware controls can change while the display is paused.
    unit.value = 'dBm';
    changeUnit();
    assert.equal(reads, 1);
    assert.equal(plot.unit, 'dBm');
    assert.ok(Math.abs(plot.plot_data[1311][1] - 10 * Math.log10(retainedPSD * 1.5 * 250e6 / 8192 / 1e-3)) < 1e-9);
    assert.equal(fields.get('connection-status').textContent, 'Display paused');
    unit.value = 'nV-Hz';
    changeUnit();
    assert.ok(Math.abs(plot.plot_data[1311][1] - Math.sqrt(50 * retainedPSD) * 1e9) < 1e-6);
    unit.value = 'dBm-Hz';
    changeUnit();
    psd[1311] = retainedPSD;
    fft.status.fs = 250e6;
    fft.status.W2 = .375;
    fft.status.dds_freq[0] = 1;
    assert.equal(plot.frameStatus.dds_freq[0], 40e6);
    plot.setPaused(false);
    await Promise.resolve();
    assert.equal(reads, 2);

    // A zero startup frame must not lock auto-scaling to the empty [-1, 1] range.
    let startupDraws = 0;
    let startupReady = false;
    const startupFFT = {...fft, async read_psd() {
        return startupReady ? psd : new Float32Array(4096);
    }};
    const startup = new Plot(doc, startupFFT, {...basics, redraw() { startupDraws++; }});
    await Promise.resolve();
    assert.equal(startupDraws, 0);
    assert.equal(fields.get('connection-status').textContent, 'Waiting for spectrum…');
    startupReady = true;
    await startup.updatePlot();
    assert.equal(startupDraws, 1);
    assert.equal(fields.get('connection-status').textContent, 'Live spectrum');
    startup.dispose();
    plot.dispose();
    await plot.updatePlot();
    assert.equal(reads, 2);

    // A click marker must interpolate the full spectrum, even when the displayed
    // curve has fewer points after decimation. Previously this could throw.
    const shared = Object.create(PlotBasics.prototype);
    Object.assign(shared, {
        plot: {getData: () => [{data: [[0, 1], [4, 5]]}], setData() {}, draw() {}, unhighlight() {}, highlight() {}},
        options: {legend: {}}, seriesOne: [{}], reset_range: false, decimate: false, isPeakDetection: false,
        clickDatapoint: [3.5, 0], clickDatapointSpan: {style: {}}, peakDatapointSpan: {style: {}},
        range_x: {from: 0, to: 5}, range_y: {from: 0, to: 10}, updateDatapointSpan() {}
    });
    shared.redraw([[0, 1], [1, 2], [2, 3], [3, 4], [4, 5]], 5, [], '', () => {});
    assert.equal(shared.clickDatapoint[1], 4.5);
    shared.redraw([], 0, [], '', () => {});
})()
`, context).then(() => console.log('FFT bins, peak, pause, snapshot and decimated marker: PASS'))
.catch(error => { console.error(error); process.exitCode = 1; });
