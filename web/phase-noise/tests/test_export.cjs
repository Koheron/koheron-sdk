const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const exportedAt = '2026-10-07T12:34:56.789Z';

function fixture(t, board) {
    const dom = new JSDOM(`<body data-board="${board}"><div id="plot-placeholder"><canvas class="flot-base"></canvas></div></body>`, {runScripts: 'outside-only'});
    const w = dom.window, d = w.document;
    t.after(() => w.close());
    d.title = `Phase noise · ${board}`;
    d.body.insertAdjacentHTML('beforeend', read('web/phase-noise/export-file/export-file.html').replace(/<\/?template[^>]*>/g, ''));
    const adapter = board === 'alpha250-4'
        ? 'examples/alpha250-4/phase-noise-analyzer/web/export-file/export-file.ts'
        : 'web/phase-noise/analyzer/export-file/export-file.ts';
    w.eval(ts.transpileModule(read('web/phase-noise/export-file/export-file.ts') + '\n' + read(adapter),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.ExportFile = ExportFile;');
    w.Date = class extends Date { constructor() { super(exportedAt); } };
    w.Blob = Blob;
    const downloads = [], revoked = [], timers = [], drawing = [];
    w.URL.createObjectURL = blob => { downloads.push({blob}); return 'blob:export'; };
    w.URL.revokeObjectURL = url => revoked.push(url);
    w.HTMLAnchorElement.prototype.click = function () { Object.assign(downloads.at(-1), {name: this.download, url: this.href}); };
    w.setTimeout = (callback, delay) => { timers.push({callback, delay}); return timers.length; };
    const canvas = d.querySelector('canvas');
    canvas.width = 1200; canvas.height = 600;
    Object.defineProperty(canvas, 'clientWidth', {value: 600});
    w.HTMLCanvasElement.prototype.getContext = () => ({
        measureText: text => ({width: text.length * 6}),
        scale: (...args) => drawing.push(['scale', ...args]),
        fillRect: (...args) => drawing.push(['fillRect', ...args]),
        fillText: (...args) => drawing.push(['fillText', ...args]),
        drawImage: (source, ...args) => { assert.equal(source, canvas); drawing.push(['drawImage', ...args]); }
    });
    w.HTMLCanvasElement.prototype.toBlob = function (callback, type) {
        drawing.push(['image', this.width, this.height, type]);
        callback(new Blob(['PNG'], {type}));
    };
    const parameters = {channel: board === 'alpha250-4' ? 2 : 1, fs: 1e6, clkIndex: '2',
        fdds0: 10e6, fdds1: 20e6, fdds2: 30e6, fdds3: 40e6,
        cic_rate: 100, fft_navg: 8, avgxy_count: 64, analyzer_mode: 'RF', interferometer_delay: 1e-9};
    const plot = {frameStatus: parameters, frameReceivedAt: 'live-time', yLabel: 'Phase noise (dBc/Hz)',
        plot_data: [[100, -120], [200, NaN]], smooth_plot_data: [[100, -121], [200, Infinity]],
        phase_psd: new Float32Array([-2, 4]), referenceParameters: {...parameters, channel: 0, fs: 2e6},
        referenceReceivedAt: 'reference-time', reference_data: [[300, -130]],
        reference_smooth_data: [], referencePSD: new Float32Array([-8]),
        plotBasics: {plot: {getData: () => [{label: 'Live', color: '#019cd5'}, {label: 'Reference', color: '#a178b5'}, {color: 'ignored'}]}}};
    const exporter = new w.ExportFile(d, plot);
    return {w, d, plot, exporter, downloads, revoked, timers, drawing, canvas};
}

for (const board of ['alpha250', 'alpha250-4', 'red-pitaya', 'dpll']) {
    test(`${board}: CSV retains captured metadata, signed PSD, blank nonfinite values and reference grid`, async t => {
        const h = fixture(t, board);
        h.d.querySelector('.export-data').click();
        assert.equal(h.downloads.length, 1);
        const download = h.downloads[0], csv = await download.blob.text();
        assert.equal(download.blob.type, 'text/csv;charset=utf-8');
        assert.equal(download.name, `phase-noise-${board}-2026-10-07T12-34-56-789Z.csv`);
        assert.equal(download.url, 'blob:export');
        assert.ok(csv.startsWith(`Phase noise · ${board}\n"Exported at",${exportedAt}\n\n"Frame received at",live-time\n`));
        assert.ok(csv.includes('"Sampling frequency (Hz)",1000000\n'));
        assert.ok(csv.includes(`"Reference clock",${board === 'red-pitaya' ? 'Fixed onboard' : 'Internal'}\n`));
        assert.ok(csv.includes('100,-120,-121,-2\n200,,,4\n'));
        const reference = csv.split('\nReference trace\n')[1];
        assert.ok(reference.startsWith('"Frame received at",reference-time\n"Input channel",0\n"Sampling frequency (Hz)",2000000\n'));
        assert.ok(reference.endsWith('300,-130,,-8\n'));
        if (board === 'alpha250-4') {
            assert.ok(csv.includes('"LO 2 frequency (Hz)",30000000\n"LO 3 frequency (Hz)",40000000\n'));
            assert.ok(csv.includes('"Cumulative XY segments",64\n'));
            assert.ok(csv.includes('"Signed phase PSD (rad^2/Hz)"'));
            assert.ok(!csv.includes('"Analyzer mode"'));
        } else {
            assert.ok(csv.includes('"Analyzer mode",RF\n"Interferometer delay (s)",1e-9\n'));
            assert.ok(csv.includes('"Phase PSD (rad^2/Hz)"'));
            assert.ok(!csv.includes('"LO 2 frequency (Hz)"'));
        }
        assert.equal(h.timers[0].delay, 1000);
        h.timers[0].callback();
        assert.deepEqual(h.revoked, ['blob:export']);
    });

    test(`${board}: PNG keeps HiDPI pixels, captured labels and both legend entries`, t => {
        const h = fixture(t, board);
        h.d.querySelector('.export-plot').click();
        assert.equal(h.downloads[0].blob.type, 'image/png');
        assert.equal(h.downloads[0].name, `phase-noise-${board}-2026-10-07T12-34-56-789Z.png`);
        assert.deepEqual(h.drawing[0], ['scale', 2, 2]);
        assert.deepEqual(h.drawing.find(row => row[0] === 'drawImage'), ['drawImage', 0, 112, 600, 300]);
        assert.deepEqual(h.drawing.at(-1), ['image', 1200, 884, 'image/png']);
        const labels = h.drawing.filter(row => row[0] === 'fillText').map(row => row[1]);
        assert.ok(labels.includes(`Phase noise (dBc/Hz) · ${board}`));
        assert.ok(labels.includes(board === 'alpha250-4'
            ? 'Live · XY · CIC 100 · 64 cumulative segments'
            : 'Live · ADC 1 · CIC 100 · averaging window 8 · RF'));
        assert.ok(labels.includes(board === 'alpha250-4'
            ? 'Reference · X · CIC 100 · averaging window 8'
            : 'Reference · ADC 0 · CIC 100 · averaging window 8 · RF'));
        assert.ok(labels.includes('Received live-time'));
        assert.ok(labels.includes('Live') && labels.includes('Reference'));
        assert.equal(labels.at(-1), 'Offset frequency (Hz)');
    });
}

test('exports remain disabled without a captured frame and omit an absent reference', async t => {
    const h = fixture(t, 'alpha250-4');
    h.plot.frameStatus = undefined;
    new h.w.ExportFile(h.d, h.plot);
    assert.ok(h.d.querySelector('.export-data').disabled);
    assert.ok(h.d.querySelector('.export-plot').disabled);
    h.exporter.exportData(); h.exporter.exportPlot();
    assert.equal(h.downloads.length, 0);
    h.plot.frameStatus = {channel: 0};
    h.plot.referenceParameters = undefined;
    h.exporter.exportData();
    assert.ok(!(await h.downloads[0].blob.text()).includes('Reference trace'));
});
