// Run with NODE_PATH pointing to the SDK node_modules in an isolated worktree.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const source = path.resolve(__dirname, '../web/export-file/export-file.ts');
const context = vm.createContext({assert, console, Blob});
vm.runInContext(ts.transpileModule(fs.readFileSync(source, 'utf8'), {
    compilerOptions: {target: ts.ScriptTarget.ES2020}
}).outputText, context);
vm.runInContext(`
(async () => {
    const spectrum = {
        yLabel: 'Voltage noise (nV/√Hz)',
        frameStatus: {channel: 1, window_index: 1, fs: 250e6, clkIndex: '2', dds_freq: [40e6, 0]},
        plot_data: [[0, 0], [40.008544921875, 123.456]]
    };
    for (const density of [1, 2]) {
        const buttons = new Map();
        const text = [];
        let scaled, copied, background, image, download;
        const canvas = {width: 320 * density, height: 200 * density, clientWidth: 320};
        const ctx = {
            scale(x, y) { scaled = [x, y]; },
            fillRect(...args) { background = args; },
            fillText(value, x, y) { text.push({value, x, y, font: this.font}); },
            drawImage(...args) { copied = args; }
        };
        const doc = {
            querySelector(selector) {
                if (selector.includes('canvas')) return canvas;
                return {addEventListener(event, cb) { buttons.set(selector, cb); }};
            },
            createElement() { return image = {
                getContext() { return ctx; },
                toBlob(cb) { cb(new Blob(['image'], {type: 'image/png'})); }
            }; }
        };
        const exporter = new ExportFile(doc, spectrum);
        exporter.download = (blob, name) => { download = {blob, name}; };
        buttons.get('.export-plot')();
        assert.equal(image.width, 320 * density);
        assert.equal(image.height, 282 * density);
        assert.deepEqual(scaled, [density, density]);
        assert.deepEqual(copied, [canvas, 0, 52, 320, 200]);
        assert.deepEqual(background, [0, 0, 320, 282]);
        assert.equal(text[0].value, 'ALPHA250 FFT · Voltage noise (nV/√Hz)');
        assert.equal(text[1].value, 'ADC 1 · Hann · 250 MS/s');
        assert.equal(text[2].value, 'Frequency (MHz)');
        assert.equal(download.name, 'koheron_fft.png');
        buttons.get('.export-data')();
        const csv = await download.blob.text();
        assert.equal(download.name, 'koheron_fft.csv');
        assert.ok(csv.includes('Input channel,1'));
        assert.ok(csv.includes('Sampling frequency (Hz),250000000'));
        assert.ok(csv.includes('Reference clock,Internal'));
        assert.ok(csv.includes('Frequency (MHz),Voltage noise (nV/√Hz)'));
        assert.ok(csv.endsWith('0,0\\n40.008544921875,123.456'));
        spectrum.frameStatus.clkIndex = '0';
        buttons.get('.export-data')();
        assert.ok((await download.blob.text()).includes('Reference clock,External'));
        spectrum.frameStatus.clkIndex = '2';
    }
})()
`, context).then(() => console.log('PNG density, displayed-frame labels and CSV values: PASS'))
.catch(error => { console.error(error); process.exitCode = 1; });
