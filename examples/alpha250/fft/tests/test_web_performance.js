// Verify telemetry cadence and frame budgets without real timers or hardware.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const root = path.resolve(__dirname, '../../../..');
const context = vm.createContext({console, assert});
for (const file of ['examples/alpha250/fft/web/fft/fft-app.ts', 'examples/alpha250/fft/web/plot/plot.ts']) {
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
        _lastControlsTick: 0, _lastBoardTick: -Infinity, _supplySpans: [], _temperatureSpans: [],
        ensureControlsCache() {}, setCheckedIfNeeded() {}, setValueIfNeeded() {},
        driver: {
            async getControlParameters() { controls++; return {fs: 250e6, channel: 0, window_index: 1, clkIndex: '0'}; },
            async getBoardParameters() { board++; return {adcValues: [0,0,0,0], dacValues: [0,0,0,0]}; }
        }
    });
    for (const time of [250, 500, 750, 1000, 1250]) { now = time; await widget.updateControls(); }
    assert.equal(controls, 5);
    assert.equal(board, 2); // Initial telemetry, then at one second; controls remain 4 Hz.
    widget.dispose(); now += 1000; await widget.updateControls();
    assert.equal(controls, 5);

    let scheduled, reads = 0, finishRead;
    const plot = Object.assign(Object.create(Plot.prototype), {
        running: true, paused: false, busy: false,
        fft: {status: {dds_freq: [0,0]}, read_psd() { reads++; return new Promise(resolve => { finishRead = resolve; }); }},
        displaySpectrum() { now += 6; }, setStatus() {}, schedule(delay) { scheduled = delay; }
    });
    now = 0;
    const frame = plot.updatePlot();
    await plot.updatePlot();
    assert.equal(reads, 1); // Only one acquisition can be in flight.
    now = 4; finishRead(new Float32Array([1])); await frame;
    assert.equal(scheduled, 40); // 4 ms acquisition + 6 ms drawing + 40 ms wait.
    now = 100;
    const slowFrame = plot.updatePlot(); now = 160; finishRead(new Float32Array([1])); await slowFrame;
    assert.equal(scheduled, 0); // Slow frames do not add another 50 ms.
    plot.fft.read_psd = async () => { throw new Error('expected acquisition failure'); };
    const savedError = console.error; console.error = () => {};
    await plot.updatePlot(); console.error = savedError;
    assert.equal(scheduled, 1000); // Preserve retry backoff.
    plot.paused = true; await plot.updatePlot(); assert.equal(reads, 2);
})()
`, context).then(() => console.log('Telemetry cadence, frame budget, overlap and retry backoff: PASS')).catch(error => {
    console.error(error); process.exitCode = 1;
});
