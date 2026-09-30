const fs = require('fs');
const vm = require('vm');
const assert = require('assert');

// Load the compiled application without starting browser connections.
const context = vm.createContext({
    console, window: {addEventListener() {}}, document: {},
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
    console.log('Web decoders passed against C++ serialized payloads');
})().catch(e => { console.error(e); process.exitCode = 1; });
