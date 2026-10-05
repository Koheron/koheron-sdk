// Verify the Red Pitaya wire adapter used by the shared FFT workspace.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const root = path.resolve(__dirname, '../../../..');
const context = vm.createContext({console, assert});
for (const file of ['web/koheron.ts', 'web/fft/driver.ts', 'examples/red-pitaya/fft/web/fft.ts']) {
    vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText, context);
}
vm.runInContext(`
(async () => {
    const wire = new DataView(new ArrayBuffer(44));
    wire.setFloat64(0, 5e6); wire.setFloat64(8, 10e6);
    wire.setFloat64(16, 125e6); wire.setUint32(24, 1);
    wire.setFloat64(28, .25); wire.setFloat64(36, .375);
    for (let window = 0; window < 4; window++) {
        const client = {
            getDriver: () => ({id: 5, getCmds: () => ({
                get_control_parameters: {id: 9, args: []},
                get_window_index: {id: 10, args: []},
                get_fft_size: {id: 11, args: []}
            })}),
            readTuple: async (_, fmt) => Client.prototype.deserialize(fmt, wire),
            readUint32: async () => window
        };
        const fft = new FFT(client);
        const status = await fft.getControlParameters();
        assert.equal(status.fs, 125e6);
        assert.equal(status.window_index, window);
        assert.equal(status.channel, 1);
        assert.equal(status.clkIndex, 'fixed');
        assert.deepEqual(Array.from(status.dds_freq), [5e6, 10e6]);
        assert.equal(status.W1, .25); assert.equal(status.W2, .375);
    }
})()
`, context).then(() => console.log('Red Pitaya 44-byte FFT controls and window decoding: PASS'))
.catch(error => { console.error(error); process.exitCode = 1; });
