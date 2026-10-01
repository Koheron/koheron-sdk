const fs = require('fs');
const vm = require('vm');
const assert = require('assert');

const timers = [];

// Load the compiled application without starting browser connections.
const context = vm.createContext({
    console, setTimeout(fn, delay) { timers.push({fn, delay}); }, window: {addEventListener() {}}, document: {},
    location: {hostname: 'localhost'}, $() { return {}; },
});
vm.runInContext(fs.readFileSync(process.argv[2], 'utf8'), context);
const decode = vm.runInContext('Client.prototype.deserialize', context);
function dataView(path) {
    const b = fs.readFileSync(path);
    return new DataView(b.buffer, b.byteOffset, b.byteLength);
}
const client = {
    getDriver() { return {id: 1, getCmds() { return {get_measurements: {id: 1, args: [{type: "uint32_t"}]}, get_tracking_parameters: {id: 2, args: []}}; }}; },
    async readTuple(command, format) {
        return decode.call({}, format, dataView(format.startsWith('?') ? process.argv[4] : process.argv[3]));
    },
};
const Analyzer = vm.runInContext('PhaseNoiseAnalyzer', context);
function near(actual, expected) { assert(Math.abs(actual - expected) <= 1e-7 * Math.max(Math.abs(expected), 1e-11)); }
(async () => {
    const driver = new Analyzer(client);
    const m = await driver.getMeasurements(1);
    near(m.phase_jitter, 0.001); near(m.time_jitter, 1e-11);
    near(m.freq_lo, 100); near(m.freq_hi, 1e6); near(m.carrier_power, -3);
    const t = await driver.getTrackingParameters();
    assert.strictEqual(t.tracking_enabled, true);
    assert.strictEqual(t.tracking_locked, true);
    near(t.tracking_bandwidth, 0.1); near(t.effective_tracking_bandwidth, 0.025);
    near(t.tracking_correction_x, 0.637); near(t.tracking_correction_y, -0.125);
    near(t.tracking_last_mean_dphi, 0.01); near(t.tracking_last_error, 0.002);
    const PlotBasics = vm.runInContext('PlotBasics', context);
    const basics = Object.create(PlotBasics.prototype);
    basics._decimated = [];
    basics.plot = {
      getPlaceholder() { return {width() { return 10; }}; },
      getPlotOffset() { return {left: 0, right: 0}; },
      getAxes() { return {yaxis: {min: 0}}; },
      pointOffset({x}) { return {left: x}; }
    };
    const source = [[0, 5], [.1, 10], [.2, -4], [.3, NaN], [.4, 2], [.5, 6], [1, NaN], [1.5, 4], [1.7, 3]];
    const reduced = basics.decimateToCanva(source, 0, 2);
    for (let i = 1; i < reduced.length; i++) assert(reduced[i][0] >= reduced[i-1][0]);
    assert(reduced.some(row => row[0] === .3 && Number.isNaN(row[1])));
    assert(reduced.some(row => row[0] === 1 && Number.isNaN(row[1])));
    for (const value of [10, -4, 2, 6, 4, 3]) assert(reduced.some(row => row[1] === value));
    const dense = Array.from({length: 10000}, (_, i) => [i / 1000, Math.sin(i)]);
    assert(basics.decimateToCanva(dense, 0, 10).length <= 20);

    // Requested log range must preserve both boundary bins even while Flot
    // still has the old startup/zoom axes.
    basics.log_x = true;
    const edges = [[10, 20], [100, -5], [1000, -10]];
    const logReduced = basics.decimateToCanva(edges, 10, 1000);
    assert(logReduced.some(row => row[0] === 10 && row[1] === 20));
    assert(logReduced.some(row => row[0] === 1000 && row[1] === -10));

    const Plot = vm.runInContext('Plot', context);
    const plot = Object.create(Plot.prototype);
    plot.n_pts = 4;
    plot.plot_data = [[0, NaN], [99, 0], [100, NaN], [101, 0]];
    plot.linear_plot_data = [[0, NaN], [99, 1.5], [100, -1], [101, 0.5]];
    plot.smooth_plot_data = plot.plot_data.map(row => [row[0], NaN]);
    plot.plotBasics = {x_min: 99, x_max: 101};
    plot.computeSmoothedPlot(2);
    near(plot.smooth_plot_data[2][1], 10 * Math.log10(1 / 3));
    near(plot.getDecadeValues()[0][1], 10 * Math.log10(1 / 3));
    plot.linear_plot_data[2][1] = -4;
    plot.computeSmoothedPlot(2);
    near(plot.smooth_plot_data[2][1], 10 * Math.log10(2 / 3));
    assert(plot.negative_smooth_data.some(row => row[0] === 100));
    assert(Number.isNaN(plot.getDecadeValues()[0][1]));

    plot.laserPlotType = 'phase';
    plot.computeDisplaySpectrum(new Float32Array([0, 3, -8, 1]), 2);
    near(plot.plot_data[2][1], 10 * Math.log10(4));
    assert.strictEqual(plot.linear_plot_data[2][1], -4);
    assert.strictEqual(plot.negative_plot_data.length, 1);
    plot.laserPlotType = 'frequency';
    plot.computeDisplaySpectrum(new Float32Array([0, 3, -8, 1]), 2);
    near(plot.plot_data[2][1], 10 * Math.log10(80000));
    assert.strictEqual(plot.linear_plot_data[2][1], -80000);
    plot.computeDisplaySpectrum(new Float32Array([0, 0, NaN, Infinity]), 2);
    assert(plot.plot_data.every(row => Number.isNaN(row[1])));
    assert.strictEqual(plot.negative_plot_data.length, 0);

    const ControlApp = vm.runInContext('PhaseNoiseAnalyzerApp', context);
    const editApp = Object.create(ControlApp.prototype);
    function element(value = '') {
      return {value, handlers: {}, addEventListener(name, fn) { this.handlers[name] = fn; },
        checkValidity() { return this.value !== '' && Number.isFinite(Number(this.value)) && Number(this.value) >= 0 && Number(this.value) <= 100; }};
    }
    const minField = element('100');
    context.document.getElementsByClassName = () => [minField];
    editApp.ddsInputs = [0, 1, 2, 3].map(() => element('10.000000000'));
    editApp.ddsSetButtons = [0, 1, 2, 3].map(() => element());
    editApp.trackingEnabledInput = element();
    const loCalls = [];
    editApp.driver = {setLocalOscillator(channel, frequency) { loCalls.push([channel, frequency]); }};
    editApp.updateControls = () => {};
    editApp.initMinFrequencyInput();
    const edited = editApp.ddsInputs[2];
    edited.handlers.focus(); edited.value = '10.000000001'; edited.handlers.input();
    edited.handlers.keydown({key: 'Enter', preventDefault() {}});
    edited.handlers.blur(); editApp.ddsSetButtons[2].handlers.click();
    assert.strictEqual(loCalls.length, 1);
    assert.strictEqual(loCalls[0][0], 2); near(loCalls[0][1], 10000000.001);
    for (const invalid of ['', '-1', '101', 'NaN']) {
      edited.handlers.focus(); edited.value = invalid; edited.handlers.input(); edited.handlers.blur();
      assert.strictEqual(loCalls.length, 1);
      assert.strictEqual(editApp.isEditingDdsInputs, true);
      edited.handlers.keydown({key: 'Escape', preventDefault() {}});
      assert.strictEqual(edited.value, '10.000000001');
      assert.strictEqual(editApp.isEditingDdsInputs, false);
    }
    edited.handlers.focus(); edited.value = '10.1'; edited.handlers.input(); edited.handlers.blur();
    assert.strictEqual(loCalls.length, 2); near(loCalls[1][1], 10100000);

    // Repeated control refreshes while one request or timer is pending must
    // produce one polling chain. The real browser events call this method.
    const App = vm.runInContext('PhaseNoiseAnalyzerApp', context);
    const app = Object.create(App.prototype);
    let requests = 0;
    let release;
    const pending = new Promise(resolve => { release = resolve; });
    const parameters = {channel: 2, min_freq: 66.67, avgxy_count: 42,
      fdds0: 1e7, fdds1: 1e7, fdds2: 1e7, fdds3: 1e7, clkIndex: '2'};
    app.driver = {
      async getParameters() { requests++; await pending; return parameters; },
      async getTrackingParameters() { return {tracking_enabled: true,
        effective_tracking_bandwidth: 0.1, tracking_correction_x: 0,
        tracking_correction_y: 0}; }
    };
    app.channelInputs = [{}, {}, {}];
    app.ddsInputs = [{}, {}, {}, {}];
    for (const name of ['minFrequencyInput', 'nAvgInput', 'trackingEnabledInput',
      'trackingEffectiveBandwidthSpan', 'trackingCorrectionXSpan', 'trackingCorrectionYSpan']) app[name] = {};
    app.resetCumulativeAveragerBtn = {style: {}};
    context.document.querySelector = () => ({});
    const first = app.updateControls();
    await app.updateControls();
    assert.strictEqual(requests, 1);
    release(); await first;
    assert.strictEqual(timers.length, 1);
    assert.strictEqual(timers[0].delay, 250);
    await app.updateControls();
    assert.strictEqual(requests, 1);
    timers.shift().fn();
    await new Promise(resolve => setImmediate(resolve));
    assert.strictEqual(requests, 2);
    assert.strictEqual(timers.length, 1);
    timers.length = 0;

    // Export must retain negative raw cross estimates and all four DDS values.
    let exportClick;
    const link = {click() {}};
    const button = {addEventListener(event, fn) { exportClick = fn; },
      parentElement: {getElementsByTagName() { return [link]; }}};
    context.document = {
      getElementsByClassName(name) { return name === 'export-data' ? [button] : []; },
      querySelector(selector) {
        if (selector.includes('setReferenceClock')) return {dataset: {valuestr: 'Internal'}};
        if (selector.includes('channel')) return {value: '2'};
        return {value: '64'};
      },
      querySelectorAll(selector) {
        assert.strictEqual(selector, '.dds-input');
        const html = fs.readFileSync('examples/alpha250-4/phase-noise-analyzer/web/dds-frequency/dds-frequency.html', 'utf8');
        assert.strictEqual((html.match(/class="dds-input dds-input[0-3]"/g) || []).length, 4);
        return [0, 1, 2, 3].map(i => ({value: (10 + i).toString()}));
      }
    };
    const ExportFile = vm.runInContext('ExportFile', context);
    new ExportFile(context.document, {yLabel: 'PHASE NOISE MAGNITUDE (dBc/Hz)',
      plot_data: [[100, -140]], signed_phase_psd: [-2e-14]});
    exportClick();
    const csv = decodeURI(link.href);
    assert(csv.includes('ALPHA250-4'));
    assert(csv.includes('Channel 3 DDS frequency (MHz)",13'));
    assert(csv.includes('SIGNED PHASE PSD (rad^2/Hz)'));
    assert(csv.includes('PHASE NOISE MAGNITUDE (dBc/Hz)'));
    assert(csv.includes('100,-140,-2e-14'));
    console.log('Web payloads, signed averaging, plot gaps/order, polling and exports passed');
})().catch(e => { console.error(e); process.exitCode = 1; });
