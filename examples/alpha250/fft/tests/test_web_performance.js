// Verify telemetry cadence and frame budgets without real timers or hardware.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const root = path.resolve(__dirname, '../../../..');
const context = vm.createContext({console, assert});
for (const file of ['web/power-monitor/readout.ts', 'web/temperature-sensor/readout.ts', 'web/fft/controls/fft-app.ts', 'web/fft/plot/spectrum-history.ts', 'web/fft/plot/plot.ts', 'web/plot-basics/plot-basics.ts']) {
    vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText, context);
}
vm.runInContext(`
(async () => {
    let now = 250, controls = 0, board = 0;
    globalThis.performance = {now: () => now};
    globalThis.setTimeout = () => 1;
    globalThis.document = {activeElement: null, querySelector: () => ({textContent: ''}), querySelectorAll: () => []};
    const widget = Object.assign(Object.create(FFTApp.prototype), {
        running: true, channelNum: 0, _busyControls: false, _controlsHz: 4,
        _lastControlsTick: 0, document: globalThis.document, _supplySpans: [], _temperatureSpans: [],
        ensureControlsCache() {}, setCheckedIfNeeded() {}, setValueIfNeeded() {},
        driver: {
            async getControlParameters() { controls++; return {fs: 250e6, channel: 0, window_index: 1, clkIndex: '0'}; },
            async getBoardParameters() { board++; return {adcValues: [0,0,0,0], dacValues: [0,0,0,0]}; }
        }
    });
    for (const time of [250, 500, 750, 1000, 1250]) { now = time; await widget.updateControls(); }
    assert.equal(controls, 5);
    assert.equal(board, 0); // Slow board telemetry has its own shared poller.
    // Red Pitaya has no ALPHA250 precision-I/O telemetry endpoint.
    delete widget.driver.getBoardParameters;
    now += 1000; await widget.updateControls();
    assert.equal(controls, 6);
    assert.equal(board, 0);
    widget.dispose(); now += 1000; await widget.updateControls();
    assert.equal(controls, 6);

    let scheduled, reads = 0, finishRead, animation, animationRequests = 0, drawn = [];
    globalThis.window = {clearTimeout() {}, cancelAnimationFrame() { animation = undefined; },
        setTimeout() { return 1; },
        requestAnimationFrame(fn) { animationRequests++; animation = fn; return 1; }};
    const fields = new Map();
    const doc = {hidden: false, getElementById(id) {
        if (!fields.has(id)) fields.set(id, {textContent: '', dataset: {}});
        return fields.get(id);
    }};
    const status = {dds_freq: [10,0], fs: 250e6};
    const plot = Object.assign(Object.create(Plot.prototype), {
        document: doc, history: new SpectrumHistory(), running: true, paused: false, busy: false, animation: 0,
        lastFrameTime: -Infinity, rateStarted: 0, acquiredFrames: 0, renderedFrames: 0, paintedFrames: 0,
        plotBasics: {needsRedraw: () => false},
        fft: {status, read_psd() { reads++; return new Promise(resolve => { finishRead = resolve; }); }},
        displaySpectrum() { drawn.push({psd: Array.from(this.psd), status: this.frameStatus}); },
        setStatus() {}, schedule(delay) { scheduled = delay; }
    });
    const raw = new Float32Array([1]);
    now = 0; const frame = plot.updatePlot(); await plot.updatePlot();
    assert.equal(reads, 1); // Only one acquisition can be in flight.
    now = 4; finishRead(raw); await frame;
    assert.ok(Math.abs(scheduled - (1000/60 - 4)) < 1e-9);
    assert.equal(drawn.length, 0); // Network completion never draws outside an animation frame.
    assert.equal(animationRequests, 1);
    assert.equal(plot.history.samples, 1);
    raw[0] = 2; status.dds_freq[0] = 20;
    now = 17; const next = plot.updatePlot(); now = 21; finishRead(raw); await next;
    assert.equal(plot.history.samples, 2); // History consumes every received frame before paint.
    assert.equal(reads, 2); // Acquisition progresses while rendering waits.
    assert.equal(animationRequests, 1); // Only one paint callback can be queued.
    raw[0] = 3; status.dds_freq[0] = 30;
    now = 33.4; animation(33.4);
    assert.equal(drawn.length, 1);
    assert.deepEqual(drawn[0].psd, [2]); // Only the newest complete sample is painted.
    assert.equal(drawn[0].status.dds_freq[0], 20); // Own both samples and metadata.
    assert.equal(plot.pending, undefined);
    now = 34; const fast = plot.updatePlot(); finishRead(raw); await fast;
    animation(41.7); assert.equal(drawn.length, 1); // Cap paint at 60 Hz on faster monitors.
    animation(50.1); assert.equal(drawn.length, 2);
    const freshFrames = plot.renderedFrames;
    const historySamples = plot.history.samples;
    const paintRequests = animationRequests;
    now = 68; plot.acceptSpectrum(new Float32Array([3]), now/1000);
    assert.equal(animationRequests, paintRequests); // Cached PSD needs no paint.
    assert.equal(plot.history.samples, historySamples + 1); // Preserve received-history timing.
    assert.equal(plot.renderedFrames, freshFrames);
    // An undrawn transient must not be painted after the newest reply returns
    // to the displayed PSD; count changed spectra only when actually shown.
    plot.acceptSpectrum(new Float32Array([4]), .07);
    plot.acceptSpectrum(new Float32Array([3]), .071);
    assert.equal(plot.pending, undefined);
    now = 72; animation(now);
    assert.equal(drawn.length, 2);
    // Metadata, overlays, history scrolling and resized axes still redraw a
    // cached spectrum, without inflating the changed-spectrum FPS counter.
    status.dds_freq[0] = 31;
    plot.acceptSpectrum(new Float32Array([3]), .073);
    now = 74; animation(now);
    assert.equal(drawn.at(-1).status.dds_freq[0], 31);
    assert.equal(plot.renderedFrames, freshFrames);
    fields.get('average-trace').checked = true;
    plot.acceptSpectrum(new Float32Array([3]), .075);
    now = 92; animation(now);
    assert.equal(plot.renderedFrames, freshFrames);
    fields.get('average-trace').checked = false;
    fields.get('max-hold-trace').checked = true;
    plot.acceptSpectrum(new Float32Array([3]), .093);
    now = 110; animation(now);
    assert.equal(plot.renderedFrames, freshFrames);
    fields.get('max-hold-trace').checked = false;
    plot.views = {mode: 'spectrogram'};
    plot.acceptSpectrum(new Float32Array([3]), .111);
    now = 128; animation(now);
    assert.equal(plot.renderedFrames, freshFrames); // Time-axis scrolling is still painted.
    plot.views = undefined;
    plot.plotBasics.needsRedraw = () => true;
    plot.acceptSpectrum(new Float32Array([3]), .129);
    now = 146; animation(now);
    assert.equal(plot.renderedFrames, freshFrames); // A cached PSD honors resized/selected axes.
    plot.plotBasics.needsRedraw = () => false;
    const connection = doc.getElementById('connection-status');
    connection.dataset.state = 'error'; connection.textContent = 'Disconnected';
    plot.setStatus = Plot.prototype.setStatus;
    plot.acceptSpectrum(new Float32Array([3]), .147);
    assert.equal(connection.dataset.state, 'live'); // Recovery can return the same valid PSD.
    assert.equal(connection.textContent, 'Live spectrum');
    plot.setStatus = () => {};
    const withGap = Object.assign(Object.create(Plot.prototype), {psd: new Float32Array([3, NaN])});
    assert.equal(withGap.spectrumChanged(new Float32Array([3, NaN])), false);
    assert.equal(withGap.spectrumChanged(new Float32Array([4, NaN])), true);
    assert.equal(withGap.spectrumChanged(new Float32Array([3])), true);
    const display = plot.displaySpectrum;
    plot.displaySpectrum = () => { throw new Error('expected paint failure'); };
    plot.acceptSpectrum(new Float32Array([4]), .148);
    const paintError = console.error; console.error = () => {};
    now = 165; animation(now); console.error = paintError;
    assert.deepEqual(Array.from(plot.psd), [3]); // A failed paint is not a displayed spectrum.
    assert.equal(plot.renderedFrames, freshFrames);
    assert.equal(plot.paintFailed, true);
    plot.displaySpectrum = display;
    const beforeRecovery = drawn.length;
    plot.acceptSpectrum(new Float32Array([3]), .166);
    now = 184; animation(now);
    assert.equal(plot.paintFailed, false);
    assert.equal(drawn.length, beforeRecovery + 1); // Recovery redraws even an unchanged cached PSD.
    assert.equal(plot.renderedFrames, freshFrames);
    plot.acceptSpectrum(new Float32Array([4]), .185);
    now = 202; animation(now);
    assert.equal(plot.renderedFrames, freshFrames + 1); // The previously failed PSD counts only when shown.
    now = 224; const slow = plot.updatePlot(); now = 280; finishRead(raw); await slow;
    assert.equal(scheduled, 0); // Slow reads do not add another idle interval.
    plot.setPaused(true); assert.equal(plot.pending, undefined);
    const pausedReads = reads; await plot.updatePlot(); assert.equal(reads, pausedReads);
    plot.paused = false; doc.hidden = true; await plot.updatePlot(); assert.equal(reads, pausedReads);
    doc.hidden = false;
    plot.fft.read_psd = async () => { throw new Error('expected acquisition failure'); };
    plot.pending = {psd: new Float32Array([9]), status};
    plot.animation = 1;
    const savedError = console.error; console.error = () => {};
    await plot.updatePlot(); console.error = savedError;
    assert.equal(scheduled, 1000); // Preserve retry backoff.
    assert.equal(plot.pending, undefined); // A stale paint cannot hide an acquisition error.
    assert.equal(plot.animation, 0);
    now = 2000; plot.rateStarted = 1000; plot.renderedFrames = 60; plot.paintedFrames = 60; plot.acquiredFrames = 61;
    plot.updateRate(); assert.equal(fields.get('refresh-rate').textContent, '60 FPS');
    assert.ok(fields.get('refresh-rate').title.includes('61 spectra/s'));

    const spectrum = Array.from({length: 4096}, (_, i) => [i, -100]);
    spectrum[511][1] = 20; spectrum[510][1] = -140;
    spectrum[1600][1] = NaN;
    const reduced = PlotBasics.reduceSpectrum(spectrum, 0, 4095, 320);
    assert.ok(reduced.length <= 645);
    assert.ok(reduced.includes(spectrum[511])); // Single-bin peaks cannot disappear.
    assert.ok(reduced.includes(spectrum[510])); // Preserve the noise envelope too.
    assert.ok(reduced.includes(spectrum[1600])); // No line bridges missing samples.
    assert.equal(reduced[0], spectrum[0]); assert.equal(reduced.at(-1), spectrum.at(-1));
    for (let i = 1; i < reduced.length; i++) assert.ok(reduced[i][0] > reduced[i-1][0]);
    const zoomed = PlotBasics.reduceSpectrum(spectrum, 500, 520, 320);
    assert.equal(zoomed.length, 23); // Every bin returns when zoomed in.
    assert.equal(zoomed[0][0], 499); assert.equal(zoomed.at(-1)[0], 521);
    assert.equal(spectrum.length, 4096); // Full samples remain intact for exports/cursors.

})()
`, context).then(() => console.log('Independent acquisition/paint, latest-frame ownership, FPS, telemetry and retry backoff: PASS')).catch(error => {
    console.error(error); process.exitCode = 1;
});
