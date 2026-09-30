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
    const events = new Map();
    const doc = {
        getElementById(id) {
            if (!fields.has(id)) fields.set(id, {dataset: {}, textContent: '', checked: false,
                disabled: true, addEventListener(event, fn) { events.set(id, fn); }});
            return fields.get(id);
        },
        querySelector(selector) { return selector === '.unit-input:checked' ? unit : {addEventListener() {}}; },
        querySelectorAll() { return [unit]; }
    };
    let changeUnit, zoom;
    const unit = {value: 'dBm-Hz', addEventListener(event, fn) { changeUnit = fn; }};
    globalThis.$ = () => ({on(events, fn) { zoom = fn; }, off() {}});
    globalThis.window = {setTimeout: () => 1, clearTimeout() {}, requestAnimationFrame: () => 1, cancelAnimationFrame() {}};
    globalThis.setTimeout = window.setTimeout;
    let range = {from: 0, to: 40}, drawn, reads = 0;
    const psd = new Float32Array([1e-3, 1e-6, NaN, 1e-8]);
    const fft = {fft_size: 8, status: {fs: 80e6, W1: .25, W2: .5, dds_freq: [10e6, 0], channel: 1, window_index: 1},
        async read_psd() { reads++; return psd; }};
    const basics = {enableSpectrumReduction() {}, setLinY() {}, setRangeX(from, to) { range = {from, to}; },
        getRangeX() { return {...range}; },
        redraw(data, count, peak, label, cb, reference, peakIsFinal) { drawn = {data, peak, reference, peakIsFinal}; }};
    const plot = new Plot(doc, fft, basics);
    plot.captureReference();
    assert.equal(plot.referenceStatus, undefined); // No frame yet.
    await Promise.resolve();
    assert.equal(drawn.peak[0], 0);
    assert.equal(fields.get('capture-reference').disabled, false);
    plot.setPaused(true);
    range = {from: 25, to: 35};
    zoom();
    assert.equal(drawn.peak[0], 30);
    assert.equal(fields.get('peak-frequency').textContent, '30.000000 MHz');
    assert.equal(reads, 1);
    assert.equal(drawn.peakIsFinal, true);
    range = {from: 10.1, to: 19.9};
    zoom();
    assert.equal(drawn.peak.length, 0);
    assert.equal(fields.get('peak-frequency').textContent, '—');
    range = {from: 0, to: 40};
    fields.get('exclude-dc').checked = true;
    events.get('exclude-dc')();
    assert.equal(drawn.peak[0], 10);
    assert.equal(reads, 1);
    events.get('capture-reference')();
    assert.equal(drawn.reference.length, 4);
    assert.equal(fields.get('reference-status').textContent, 'ADC 1 · Hann · 80 MS/s');
    assert.equal(fields.get('capture-reference').textContent, 'Replace ref');
    const referenceRaw = psd[1];
    psd[1] = 1e-10;
    fft.status.fs = 40e6;
    fft.status.W1 = fft.status.W2 = 1;
    fft.status.dds_freq[0] = 5e6;
    unit.value = 'dBm';
    changeUnit();
    const referencePower = 10 * Math.log10(referenceRaw * 2 * 80e6 / 8 / 1e-3);
    assert.ok(Math.abs(drawn.reference[1][1] - referencePower) < 1e-9);
    const cachedReference = drawn.reference;
    plot.setPaused(false);
    await Promise.resolve();
    assert.equal(reads, 2);
    assert.equal(drawn.data[1][0], 5);
    assert.equal(drawn.reference[1][0], 10); // Each trace retains its frequency grid.
    assert.ok(Math.abs(drawn.reference[1][1] - referencePower) < 1e-9);
    assert.equal(drawn.reference, cachedReference); // Live frames reuse the converted reference.
    assert.equal(plot.referenceStatus.fs, 80e6);
    assert.equal(plot.referenceStatus.dds_freq[0], 10e6);
    assert.ok(drawn.data[1][1] < drawn.reference[1][1]);
    unit.value = 'nV-rtHz';
    changeUnit();
    assert.notEqual(drawn.reference, cachedReference);
    const convertedReference = drawn.reference;
    plot.captureReference();
    assert.notEqual(drawn.reference, convertedReference); // Replacing a capture invalidates the cache.
    assert.equal(plot.referenceStatus.fs, 40e6);
    assert.equal(plot.referenceStatus.dds_freq[0], 5e6);
    events.get('clear-reference')();
    assert.equal(drawn.reference, undefined);
    assert.equal(plot.referenceStatus, undefined);
    assert.equal(fields.get('reference-info').hidden, true);
    assert.equal(fields.get('clear-reference').disabled, true);
    plot.captureReference(); // Replace with the latest frame.
    assert.equal(plot.referenceStatus.fs, 40e6);
    plot.dispose();

    const shared = Object.create(PlotBasics.prototype);
    const highlights = [];
    Object.assign(shared, {
        plot: {getData: () => shared.seriesOne, setData() {}, draw() {}, unhighlight() {},
            highlight(series, point) { highlights.push(point.slice()); }},
        options: {legend: {}}, seriesOne: [{}], reset_range: false, decimate: false, isPeakDetection: true,
        clickDatapoint: [], peakDatapointSpan: {style: {}},
        range_x: {from: 0, to: 40}, range_y: {from: 0, to: 100}, updateDatapointSpan() {}
    });
    const copy = shared.getRangeX(); copy.from = 20;
    assert.equal(shared.getRangeX().from, 0);
    shared.redraw([[0, 90], [30, 4]], 2, [30, 4], 'PSD', () => {}, [[0, 80]], true);
    assert.deepEqual(highlights.at(-1), [30, 4]); // Never replace the caller's scoped peak.
    assert.equal(shared.seriesOne.length, 2);
    assert.equal(shared.seriesOne[1].label, 'Reference');
    shared.clickSeriesIndex = 1;
    shared.clickDatapoint = [15, 0];
    shared.clickDatapointSpan = {style: {}};
    shared.redraw([[0, 90], [30, 4]], 2, [], 'PSD', () => {}, [[0, 10], [30, 40]], true);
    assert.equal(shared.clickDatapoint[1], 25); // Reference cursors use reference samples.
    shared.redraw([[0, 90], [30, 4]], 2, [], 'PSD', () => {});
    assert.equal(shared.clickDatapoint.length, 0);
    assert.equal(shared.clickDatapointSpan.style.display, 'none');
    assert.equal(shared.seriesOne.length, 1);
    const markerEvents = new Map();
    let prefix;
    shared.plot_placeholder = {bind(name, fn) { markerEvents.set(name, fn); }};
    shared.updateDatapointSpan = (point, span, label) => { prefix = label; };
    shared.showClickPoint();
    markerEvents.get('plotclick')({}, {}, {datapoint: [10, 4], seriesIndex: 1, series: {label: 'Channel 1'}});
    assert.equal(prefix, ''); // Existing two-channel plots do not acquire reference labels.
    assert.equal(shared.clickSeriesIndex, 0);
    markerEvents.get('plotclick')({}, {}, {datapoint: [10, 4], seriesIndex: 1, series: {label: 'Reference'}});
    assert.equal(prefix, 'Ref ');
    assert.equal(shared.clickSeriesIndex, 1);
})()
`, context).then(() => console.log('Visible peak search, DC exclusion and independent reference traces: PASS'))
.catch(error => { console.error(error); process.exitCode = 1; });
