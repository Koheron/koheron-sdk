// Received-frame statistics, bounded history, heatmap placement and exports.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const root = path.resolve(__dirname, '../../../..');
const context = vm.createContext({assert, console, Blob});
for (const file of ['plot/spectrum-history.ts', 'plot/spectrum-views.ts', 'export-file/export-file.ts']) {
    vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(root, 'examples/alpha250/fft/web', file), 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText, context);
}
vm.runInContext(`
(async () => {
    const status = {fs: 8e6, W1: 1, W2: 1, channel: 0, window_index: 0, clkIndex: '2', dds_freq: [1e6, 0]};
    const history = new SpectrumHistory();
    const a = new Float32Array([1e-12, 1e-9, 0, NaN]);
    history.add(a, status, 0);
    a[0] = 9; status.dds_freq[0] = 2e6;
    assert.equal(history.status.dds_freq[0], 1e6);
    assert.ok(Math.abs(history.rows[0].psd[0] - 1e-12) < 1e-19);
    history.add(new Float32Array([1e-10, 1e-11, 0, NaN]), status, 1);
    assert.equal(history.samples, 2); // DDS changes preserve history.
    assert.ok(Math.abs(history.average[0] - (1e-12 / Math.E + 1e-10 * (1 - 1/Math.E))) < 1e-17);
    assert.ok(Math.abs(history.maximum[1] - 1e-9) < 1e-15);
    assert.ok(Number.isNaN(history.average[3]));
    assert.equal(history.rows.length, 2); // A one-second gap is not filled with invented spectra.
    assert.equal(history.density[256 + SpectrumHistory.code(1e-9)], 1);
    history.add(new Float32Array([1e-11, 1e-11, 0, NaN]), status, 1.02);
    assert.equal(history.rows.length, 2);
    assert.equal(history.rows[1].version, 2);
    assert.ok(Math.abs(history.rows[1].psd[0] - 1e-10) < 1e-17); // Peak aggregation within 50 ms.
    history.setDuration(5);
    history.add(new Float32Array([1e-12, 1e-9, 0, NaN]), status, 6.1);
    assert.equal(history.densityFrames, 1);
    assert.equal(history.density[256 + SpectrumHistory.code(1e-11)], 0);
    history.setDuration(15);
    assert.equal(history.densityFrames, 4); // Expanding restores retained frames.
    const oldEpoch = history.epoch;
    history.add(new Float32Array([1e-12, 1e-9, 0, NaN]), {...status, channel: 1}, 6.2);
    assert.ok(history.epoch > oldEpoch);
    assert.equal(history.samples, 1);
    assert.equal(history.densityFrames, 1);
    assert.equal(history.rows.length, 1);
    for (let i = 0; i < 2100; i++) history.add(new Float32Array([1e-12]), status, 7 + i/100);
    assert.equal(history.frames.length, 2048);
    assert.ok(history.rows.length <= 600);
    history.setDuration(30);
    assert.equal(history.densityFrames, 2048);
    assert.equal(history.density[SpectrumHistory.code(1e-12)], 2048);
    history.add(new Float32Array([1e-9]), status, 100);
    assert.equal(history.densityFrames, 1);
    assert.equal(history.rows.length, 1);
    assert.equal(history.density[SpectrumHistory.code(1e-12)], 0);
    assert.equal(history.densityLow[0], SpectrumHistory.code(1e-9));
    assert.equal(history.densityHigh[0], SpectrumHistory.code(1e-9));

    const elements = new Map();
    const events = new Map();
    const drawing = [];
    const ctx = {setTransform() {}, clearRect() {}, fillRect() {}, strokeRect() {}, fillText() {}, save() {}, restore() {}, translate() {}, scale() {}, rotate() {},
        createImageData(w, h) { return {data: new Uint8ClampedArray(w*h*4)}; }, putImageData() {}, drawImage(...args) { drawing.push(args); }};
    const doc = {activeElement: null, getElementById(id) {
        if (!elements.has(id)) elements.set(id, {value: '', checked: true, setAttribute() {}, setCustomValidity(message) { this.validationMessage = message; }, addEventListener(event, fn) { events.set(id + '/' + event, fn); }, getContext() { return ctx; }, clientWidth: 300, clientHeight: 200});
        return elements.get(id);
    }, createElement() { return {getContext() { return ctx; }}; }, querySelector() { return {addEventListener() {}}; }};
    globalThis.window = {devicePixelRatio: 2};
    const convert = power => 10*Math.log10(power/1e-3);
    history.reset(); history.setDuration(5);
    history.add(new Float32Array([1e-12, 1e-9, 1e-12, 1e-12]), status, .05);
    const views = new SpectrumViews(doc, history, () => ({from: 0, to: 4}), convert, () => {}, () => {});
    views.mode = 'spectrogram'; views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.equal(elements.get('history-canvas').width, 600);
    const columns = views.texture.width;
    assert.notEqual(views.pixels[columns + Math.floor(columns/4)], views.palette[0]); // Newest row occupies its circular slot.
    assert.equal(views.pixels[0], views.palette[0]); // Missing earlier slot remains empty.
    assert.equal(drawing.length, 1); // One scaled image prevents fractional canvas seams.
    assert.equal(views.orderedPixels[0], views.pixels[columns]); // Newest row at the top.
    assert.equal(views.orderedPixels[columns], views.palette[0]); // Missing earlier row below it.
    history.add(new Float32Array([1e-11, 1e-9, 1e-12, 1e-12]), status, .2);
    views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.equal(views.pixels[2*columns], views.palette[0]); // Real time gap remains blank.
    assert.equal(views.pixels[3*columns], views.palette[0]);
    assert.equal(views.orderedPixels[columns], views.palette[0]);
    assert.equal(views.orderedPixels[2*columns], views.palette[0]);
    assert.equal(views.orderedPixels[3*columns], views.pixels[columns]); // Older received row keeps its true age.
    views.mode = 'density'; views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.ok(views.pixels.some(pixel => pixel !== views.palette[0]));
    elements.get('auto-level').checked = false; events.get('auto-level/change')();
    elements.get('color-low').valueAsNumber = -130;
    elements.get('color-high').valueAsNumber = -30;
    events.get('color-low/input')();
    assert.equal(views.low, -130);
    elements.get('color-low').valueAsNumber = -20;
    events.get('color-low/input')();
    assert.equal(views.low, -130); // Invalid ordered range never reaches the renderer.
    assert.equal(elements.get('color-low').validationMessage, 'Low must be below High');
    elements.get('color-low').valueAsNumber = -140;
    events.get('color-low/change')();
    assert.equal(elements.get('color-low').validationMessage, '');
    assert.equal(views.low, -140);

    let download;
    const spectrum = {history, view: 'spectrogram', unit: 'dBm-Hz', yLabel: 'PSD (dBm/Hz)', frameStatus: status, convertValue: convert};
    const exporter = new ExportFile(doc, spectrum);
    exporter.download = (blob, name) => { download = {blob, name}; };
    exporter.exportData();
    const csv = await download.blob.text();
    assert.equal(download.name, 'koheron_fft_spectrogram.csv');
    assert.ok(csv.includes('Age (s) / Frequency (MHz),0,1,2,3'));
    assert.ok(csv.includes('0.05,,,,')); // Missing row explicitly exported.
    assert.equal(csv.split('\\n').filter(line => /^\\d+\\.\\d+,/.test(line)).length, 100);
    spectrum.view = 'density'; exporter.exportData();
    const densityCSV = await download.blob.text();
    assert.equal(download.name, 'koheron_fft_density.csv');
    assert.ok(densityCSV.includes('Received spectra,2'));
    assert.ok(densityCSV.includes('Values,Occurrence count'));
    history.reset(); views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.equal(elements.get('history-cursor').textContent, 'Waiting for received spectra…');
})()
`, context).then(() => console.log('Linear averages, max hold, history bounds, density expiry, time gaps, heatmaps and CSV: PASS'))
.catch(error => { console.error(error); process.exitCode = 1; });
