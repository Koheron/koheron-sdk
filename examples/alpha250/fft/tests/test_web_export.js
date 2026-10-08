// Run with NODE_PATH pointing to the SDK node_modules in an isolated worktree.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('../../../../web/transpile.cjs');
const source = path.resolve(__dirname, '../../../../web/fft/export-file/export-file.ts');
const context = vm.createContext({assert, console, Blob});
vm.runInContext(ts.transpileModule(fs.readFileSync(source, 'utf8'), {
    compilerOptions: {target: ts.ScriptTarget.ES2020}
}).outputText, context);
vm.runInContext(`
(async () => {
    const spectrum = {
        visibleReferences: [],
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
            getElementById() { return canvas; },
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
        const referenceStatus = {channel: 0, window_index: 3, fs: 200e6, clkIndex: '0', dds_freq: [10e6, 0]};
        spectrum.visibleReferences = [{name:'Ref', color:'#a178b5', capturedAt:'2026-10-08T12:00:00Z', status:referenceStatus, data:[[0, 5], [25, 42]]}];
        text.length = 0;
        buttons.get('.export-plot')();
        assert.equal(image.height, 298 * density);
        assert.deepEqual(copied, [canvas, 0, 68, 320, 200]);
        assert.equal(text[1].value, 'Live · ADC 1 · Hann · 250 MS/s');
        assert.equal(text[2].value, 'Ref · ADC 0 · Blackman–Harris · 200 MS/s');
        buttons.get('.export-data')();
        const comparison = await download.blob.text();
        const referenceCSV = comparison.split('Reference trace')[1];
        assert.ok(referenceCSV.includes('Sampling frequency (Hz),200000000'));
        assert.ok(referenceCSV.includes('Window index,3'));
        assert.ok(referenceCSV.includes('Input channel,0'));
        assert.ok(referenceCSV.endsWith('0,5\\n25,42'));
        spectrum.view = 'density'; spectrum.history = {status: spectrum.frameStatus}; text.length = 0;
        buttons.get('.export-plot')();
        assert.equal(image.height, 282 * density);
        assert.deepEqual(copied, [canvas, 0, 52, 320, 200]);
        assert.equal(text.length, 2); // Heatmap already contains axes and has no reference overlay.
        assert.equal(text[0].value, 'ALPHA250 FFT · density · Voltage noise (nV/√Hz)');
        spectrum.view = 'spectrum';
        spectrum.average_data = [[0, 7], [40, 8]];
        buttons.get('.export-data')();
        assert.ok((await download.blob.text()).includes('Average (1 s linear power EMA)'));
        spectrum.average_data = undefined;
        spectrum.visibleReferences = [];
        const rpExporter = new ExportFile(doc, spectrum, 'Red Pitaya');
        rpExporter.download = (blob, name) => { download = {blob, name}; };
        spectrum.frameStatus.clkIndex = 'fixed';
        spectrum.frameStatus.fs = 125e6;
        text.length = 0;
        buttons.get('.export-plot')();
        assert.equal(text[0].value, 'Red Pitaya FFT · Voltage noise (nV/√Hz)');
        buttons.get('.export-data')();
        const rpCsv = await download.blob.text();
        assert.ok(rpCsv.startsWith('Koheron Red Pitaya FFT'));
        assert.ok(rpCsv.includes('Reference clock,Fixed onboard'));
        assert.ok(rpCsv.includes('Sampling frequency (Hz),125000000'));
        spectrum.frameStatus.clkIndex = '2';
        spectrum.frameStatus.fs = 250e6;
    }
})()
`, context).then(() => console.log('PNG density, displayed-frame labels and CSV values: PASS'))
.catch(error => { console.error(error); process.exitCode = 1; });
