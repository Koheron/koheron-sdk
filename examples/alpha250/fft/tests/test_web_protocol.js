// Run with Node.js and the repository's TypeScript dependency installed.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

const root = path.resolve(__dirname, '../../../..');
const context = vm.createContext({console});
for (const file of ['web/koheron.ts', 'web/fft/driver.ts', 'examples/alpha250/fft/web/fft.ts']) {
    const source = fs.readFileSync(path.join(root, file), 'utf8');
    vm.runInContext(ts.transpileModule(source, {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText, context);
}

vm.runInContext(`
(async () => {
    for (const reference of [0, 2]) {
        const wire = new DataView(new ArrayBuffer(52));
        wire.setFloat64(0, 5e6);
        wire.setFloat64(8, 10e6);
        wire.setFloat64(16, 250e6);
        wire.setUint32(24, 1);
        wire.setFloat64(28, .25);
        wire.setFloat64(36, .375);
        wire.setUint32(44, 3);
        wire.setUint32(48, reference);
        const client = {
            getDriver: () => ({id: 12, getCmds: () => ({get_control_parameters: {id: 9, args: []}})}),
            readTuple: async (_, fmt) => Client.prototype.deserialize(fmt, wire)
        };
        const status = await new FFT(client).getControlParameters();
        if (status.clkIndex !== (reference === 0 ? '0' : '2') ||
            status.window_index !== 3 || status.fs !== 250e6) {
            throw new Error('Incorrect control parameters: ' + JSON.stringify(status));
        }
    }
})()
`, context).then(() => {
    console.log('Web control parameter decoding: PASS');
}).catch(error => {
    console.error(error);
    process.exitCode = 1;
});
