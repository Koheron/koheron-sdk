// Decode a production C++ tracking tuple through the compiled web RPC client.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const context = vm.createContext({console, window: {addEventListener() {}}, document: {},
    location: {hostname: 'localhost'}, $() { return {}; }, setTimeout() {}, requestAnimationFrame() {}});
vm.runInContext(fs.readFileSync(process.argv[2], 'utf8'), context);
const decode = vm.runInContext('Client.prototype.deserialize', context);
const bytes = fs.readFileSync(process.argv[3]);
const payload = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
const calls = [];
const client = {
    getDriver() { return {id: 1, getCmds() { return {
        get_tracking_parameters: {id: 1, args: []},
        set_tracking_enabled: {id: 2, args: [{type: 'bool'}]}
    }; }}; },
    async readTuple(command, format) { calls.push(format); return decode.call({}, format, payload); },
    send(command) { calls.push(command); }
};
const Analyzer = vm.runInContext('PhaseNoiseAnalyzer', context);
(async () => {
    const driver = new Analyzer(client);
    const state = await driver.getTrackingParameters();
    assert.equal(calls[0], '?dddddddddd??');
    assert.equal(state.enabled, true);
    assert.equal(state.locked0, true); assert.equal(state.locked1, false);
    assert.ok(Math.abs(state.nominal0 - (10e6 + .637)) < 1e-6);
    assert.ok(Math.abs(state.correction0 + .04) < 1e-4);
    assert.equal(state.correction1, 0);
    assert.ok(Math.abs(state.error0) < 1e-5);
    assert.ok(Number.isNaN(state.error1));
    assert.ok(Math.abs(state.effectiveBandwidth - 12207.03125 / (50 * 32768)) < 1e-12);
    driver.setTrackingEnabled(false);
    console.log('C++ tracking tuple decodes through compiled browser client with precise doubles and bools');
})().catch(error => { console.error(error); process.exitCode = 1; });
