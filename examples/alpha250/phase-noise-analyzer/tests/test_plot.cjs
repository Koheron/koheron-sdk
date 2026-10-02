const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');

function fixture(t) {
    const dom = new JSDOM('<div id="plot-empty"></div><table id="decade-values-table"></table><input id="show-smoothed-trace" type="checkbox" checked>', {runScripts: 'outside-only'});
    const w = dom.window;
    t.after(() => w.close());
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../web/plot.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.Plot = Plot;');
    // Select methods without starting the constructor's polling loop.
    const plot = Object.create(w.Plot.prototype);
    const state = {range: null, redraw: null};
    plot.n_pts = 16385; plot.samplingFrequency = 5e6;
    plot.plot_data = []; plot.laserPlotType = 'phase';
    plot.plotBasics = {
        setRangeX(low, high) { state.range = [low, high]; },
        redraw(data, size, peak, label, callback, smooth) { state.redraw = {data, size, smooth}; }
    };
    plot.driver = {
        parameters: {fs: 5e6, channel: 0},
        async getPhaseNoise() { return new Float32Array(16385).fill(2); }
    };
    plot.decadeValuesTable = w.document.querySelector('table');
    plot.showSmoothedInput = w.document.querySelector('input');
    w.app = {dds: {async getDDSFreq() { return 10e6; }}};
    w.requestAnimationFrame = () => 0;
    plot._busy = false; plot._targetHz = 60; plot._lastTick = -1000;
    plot.setFreqAxis();
    return {plot, state, window: w};
}

test('real FFT axis includes DC, exact bin centers and Nyquist', t => {
    const {plot, state} = fixture(t);
    plot.setFreqAxis();
    assert.equal(plot.plot_data[0][0], 0);
    assert.equal(plot.plot_data[64][0], 64 * 5e6 / 32768);
    assert.equal(plot.plot_data.at(-1)[0], 2.5e6);
    assert.deepEqual(state.range, [2 * 5e6 / 32768, .75 * 2.5e6]);
});

test('decade table averages linear density around the reported offsets', t => {
    const {plot} = fixture(t);
    plot.setFreqAxis();
    for (let i = 0; i < plot.n_pts; i++) plot.linear_plot_data[i][1] = i >= 2 ? (i % 2 ? 1 : 10) : 1e20;
    const scale = Math.pow(10, .05);
    for (const [frequency, value] of plot.getDecadeValues()) {
        const rows = plot.linear_plot_data.filter(([f], i) => i >= 2 && f >= frequency / scale && f <= frequency * scale);
        const expected = 10 * Math.log10(rows.reduce((sum, row) => sum + row[1], 0) / rows.length);
        if (rows.length) assert.ok(Math.abs(value - expected) < 1e-10);
        else assert.ok(Number.isNaN(value));
    }
});

test('received spectrum length controls the axis and preserves density units', async t => {
    const {plot, state} = fixture(t);
    plot.n_pts = 16384; // old cached parameters must not drop the Nyquist sample
    await plot.updatePlot();
    assert.equal(plot.n_pts, 16385);
    assert.equal(state.redraw.size, 16385);
    assert.equal(state.redraw.data.at(-1)[0], 2.5e6);
    assert.ok(Number.isNaN(state.redraw.data[0][1]));
    assert.ok(Number.isNaN(state.redraw.data[1][1]));
    assert.ok(Math.abs(state.redraw.data[64][1]) < 1e-12); // 2 rad²/Hz -> 0 dBc/Hz
    assert.ok(Math.abs(state.redraw.smooth[64][1]) < 1e-12);
    plot.laserPlotType = 'frequency'; plot._busy = false; plot._lastTick = -1000;
    await plot.updatePlot();
    assert.ok(Math.abs(state.redraw.data[64][1] - 10 * Math.log10(2 * (64 * 5e6 / 32768) ** 2)) < 1e-10);
});

test('smoothing averages linear density and excludes DC, invalid bins and negative values', t => {
    const {plot} = fixture(t);
    plot.n_pts = 1025; plot.setFreqAxis();
    const density = new Float32Array(plot.n_pts);
    for (let i = 0; i < density.length; i++) density[i] = (i % 2 ? 2 : 20) + i / 1000;
    density[0] = density[1] = 1e20;
    density[61] = NaN; density[62] = -1; density[63] = Infinity;
    const raw = density.slice();
    plot.computeDisplaySpectrum(density, 2);
    plot.computeSmoothedPlot(2);
    const scale = Math.pow(10, .05);
    for (const bin of [2, 64, 200, 1024]) {
        const selected = [...raw].map((value, i) => ({value, i})).filter(({value, i}) =>
            i >= 2 && i >= bin / scale && i <= bin * scale && Number.isFinite(value) && value >= 0);
        const mean = selected.reduce((sum, {value}) => sum + value / 2, 0) / selected.length;
        assert.ok(Math.abs(plot.smooth_plot_data[bin][1] - 10 * Math.log10(mean)) < 1e-10);
    }
    assert.ok(Number.isNaN(plot.smooth_plot_data[1][1]));
    assert.deepEqual(density, raw);
});

test('smoothing visibility changes only the overlay; raw PSD stays exportable', async t => {
    const {plot, state} = fixture(t);
    plot.showSmoothedInput.checked = false;
    await plot.updatePlot();
    assert.equal(state.redraw.smooth, undefined);
    assert.equal(plot.phase_psd.length, 16385);
    assert.equal(plot.phase_psd[64], 2);
    assert.ok(Math.abs(plot.smooth_plot_data[64][1]) < 1e-12);
});

test('CSV exports raw and smoothed display values plus the original linear PSD', async t => {
    const {plot, window: w} = fixture(t);
    await plot.updatePlot();
    w.document.body.insertAdjacentHTML('beforeend', `
        <input data-command="setReferenceClock" type="radio" checked data-valuestr="internal">
        <input name="channel" type="radio" value="0" checked>
        <span><input class="dds-input" value="10000000.637"><select class="lo-unit"><option>Hz</option></select></span>
        <input class="cic-rate-input" value="20"><input class="plot-navg-input" value="1">
        <span><button class="export-data">CSV</button><a></a></span>`);
    let exported;
    w.HTMLAnchorElement.prototype.click = function () { exported = decodeURI(this.href); };
    w.eval(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../web/export-file/export-file.ts'), 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.ExportFile = ExportFile;');
    new w.ExportFile(w.document, plot);
    w.document.querySelector('.export-data').click();
    assert.ok(exported.includes('"LO 0 frequency (MHz)",10.000000637'));
    assert.ok(exported.includes('"PHASE PSD (rad^2/Hz)"'));
    const table = exported.split('"CARRIER OFFSET FREQUENCY (Hz)"')[1].trim().split('\n').slice(1);
    assert.equal(table.length, 16385);
    const row = table[64].split(',').map(Number);
    assert.equal(row[0], 64 * 5e6 / 32768);
    assert.ok(Math.abs(row[1]) < 1e-12 && Math.abs(row[2]) < 1e-12);
    assert.equal(row[3], 2);
});
