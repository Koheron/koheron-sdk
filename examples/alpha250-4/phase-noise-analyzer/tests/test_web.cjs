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

    console.log('Web payloads and shared plot gaps/order passed');
})().catch(e => { console.error(e); process.exitCode = 1; });
