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
    assert.ok(history.rows.length <= 601);
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
    const selectionBoxes = [];
    const ctx = {beginPath() {}, moveTo() {}, lineTo() {}, stroke() {}, setTransform() {}, clearRect() {}, fillRect() {}, strokeRect(...args) { selectionBoxes.push(args); }, fillText() {}, save() {}, restore() {}, translate() {}, scale() {}, rotate() {},
        createImageData(w, h) { return {data: new Uint8ClampedArray(w*h*4)}; }, putImageData() {}, drawImage(...args) { drawing.push(args); }};
    const doc = {activeElement: null, getElementById(id) {
        if (!elements.has(id)) elements.set(id, {value: '', checked: true, setAttribute() {}, setCustomValidity(message) { this.validationMessage = message; }, addEventListener(event, fn) { events.set(id + '/' + event, fn); }, getContext() { return ctx; }, setPointerCapture() {}, reportValidity() {}, getBoundingClientRect() { return {left: 0, top: 0}; }, clientWidth: 300, clientHeight: 200});
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
    assert.equal(views.texture.height, 101); // Extra row covers fractional scrolling at the bottom.
    assert.equal(drawing.at(-1)[2], 1); // At a bucket boundary its future portion is cropped.
    history.add(new Float32Array([1e-12, 1e-9, 1e-12, 1e-12]), status, .05 + 1/60);
    views.render('dBm-Hz', 'PSD (dBm/Hz)');
    const firstOffset = drawing.at(-1)[2];
    assert.ok(Math.abs(firstOffset - 2/3) < 1e-12);
    history.add(new Float32Array([1e-12, 1e-9, 1e-12, 1e-12]), status, .05 + 2/60);
    views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.ok(Math.abs(drawing.at(-1)[2] - 1/3) < 1e-12);
    assert.equal(drawing.at(-1)[4], 100); // The exact five-second window keeps its height.
    assert.equal(history.rows.length, 1); // Motion changes within a row, rather than only at 20 Hz.
    const frozenOffset = drawing.at(-1)[2];
    views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.equal(drawing.at(-1)[2], frozenOffset); // Paused redraw does not advance history time.
    history.add(new Float32Array([1e-11, 1e-9, 1e-12, 1e-12]), status, .2);
    views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.equal(views.pixels[2*columns], views.palette[0]); // Real time gap remains blank.
    assert.equal(views.pixels[3*columns], views.palette[0]);
    assert.equal(views.orderedPixels[columns], views.palette[0]);
    assert.equal(views.orderedPixels[2*columns], views.palette[0]);
    assert.equal(views.orderedPixels[3*columns], views.pixels[columns]); // Older received row keeps its true age.
    views.inspect({clientX: views.bounds.left + 1, clientY: views.bounds.top + .025 / 5 * views.bounds.height});
    assert.ok(elements.get('history-cursor').textContent.includes('No received data')); // Cursor follows fractional time position.
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
    elements.get('color-low').valueAsNumber = -20; elements.get('color-low').value = '-20';
    events.get('color-low/input')();
    views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.equal(elements.get('color-low').value, '-20'); // Invalid edit survives blur/live redraw for correction.
    events.get('color-low/keydown')({key: 'Escape'});
    assert.equal(elements.get('color-low').value, '-140');
    assert.equal(elements.get('color-low').validationMessage, '');
    elements.get('color-low').valueAsNumber = -140;
    assert.equal(elements.get('history-level-unit').textContent, 'dBm/Hz');
    events.get('history-canvas/pointerdown')({button: 0, clientX: 1, clientY: 1, pointerId: 1});
    assert.equal(views.drag, undefined); // Axes and color scale do not start a zoom.
    events.get('history-canvas/pointerdown')({button: 2, clientX: 80, clientY: 60, pointerId: 1});
    assert.equal(views.drag, undefined);
    events.get('history-canvas/pointerdown')({button: 0, clientX: 80, clientY: 60, pointerId: 1});
    assert.equal(views.drag, 80);
    events.get('history-canvas/pointermove')({clientX: 120, clientY: 60});
    assert.equal(views.pointer.clientX, 120);
    assert.deepEqual(selectionBoxes.at(-1), [80, views.bounds.top, 40, views.bounds.height]); // Visible drag preview spans the selected frequencies.
    events.get('history-canvas/pointercancel')();
    assert.equal(views.pointer, undefined);
    assert.equal(views.drag, undefined);
    views.inspect({clientX: 90, clientY: 60});
    views.inspect({clientX: 1, clientY: 1});
    assert.equal(elements.get('history-cursor').textContent, ''); // Moving to the scale clears the old reading.


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
    assert.ok(densityCSV.includes('Received spectra,4'));
    assert.ok(densityCSV.includes('Values,Occurrence count'));
    const rollingHistory = new SpectrumHistory(); rollingHistory.setDuration(5);
    const rollingViews = new SpectrumViews(doc, rollingHistory, () => ({from: 0, to: 4}), convert, () => {}, () => {});
    rollingViews.mode = 'spectrogram';
    const marker = new Float32Array([1e-9, 1e-12, 1e-12, 1e-12]);
    rollingHistory.add(marker, status, 5.025);
    rollingViews.render('dBm-Hz', 'PSD (dBm/Hz)');
    const before = drawing.at(-1)[2];
    assert.ok(Math.abs(before - .5) < 1e-12);
    rollingHistory.add(marker, status, 5.05 + 1e-14); // Ring slot wraps from 100 to 0.
    rollingViews.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.equal(rollingViews.lastBucket, 101);
    assert.ok(Math.abs((1 - drawing.at(-1)[2]) - (0 - before) - .5) < 1e-12);
    assert.equal(rollingViews.orderedPixels[rollingViews.texture.width], rollingViews.rowPixels[100][0]);
    rollingHistory.add(marker, status, 5.075);
    rollingViews.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.ok(Math.abs(drawing.at(-1)[2] - .5) < 1e-12); // No jump on the frame after wrap.
    rollingViews.pointer = {clientX: rollingViews.bounds.left + 1, clientY: rollingViews.bounds.top + .001 / 5 * rollingViews.bounds.height};
    rollingViews.render('dBm-Hz', 'PSD (dBm/Hz)');
    const hoverBefore = elements.get('history-cursor').textContent;
    rollingHistory.add(new Float32Array([1e-6, 1e-12, 1e-12, 1e-12]), status, 5.085);
    rollingViews.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.notEqual(elements.get('history-cursor').textContent, hoverBefore); // Stationary hover tracks the live data.
    history.reset(); views.render('dBm-Hz', 'PSD (dBm/Hz)');
    assert.equal(elements.get('history-cursor').textContent, 'Waiting for received spectra…');
    views.render('dBm-Hz', 'PSD (dBm/Hz)', true);
    assert.equal(elements.get('history-cursor').textContent, 'History cleared · Resume to collect spectra');
})()
`, context).then(() => console.log('Linear averages, max hold, history bounds, density expiry, time gaps, heatmaps and CSV: PASS'))
.catch(error => { console.error(error); process.exitCode = 1; });
