const assert = require('node:assert/strict');
const fs = require('node:fs');
const test = require('node:test');
const {JSDOM} = require('jsdom');
const ts = require('../../transpile.cjs');
const read = file => fs.readFileSync(file, 'utf8');
const common = ['web/plot-references/references.ts', 'web/plot-references/panel.ts',
    'web/phase-noise/references.ts', 'web/phase-noise/plot.ts'];
const parameters = signed => ({data_size:9, fs:8000, channel:signed ? 2 : 0, cic_rate:20,
    fft_navg:8, sequence:42, state:1, precision:8, average_target:8, min_freq:2000, fdds0:1e7, fdds1:2e7, clkIndex:'2', ...(signed
        ? {fdds2:3e7, fdds3:4e7, avgxy_count:64}
        : {analyzer_mode:'rf', interferometer_delay:1e-9})});

async function host(t, board) {
    const signed = board === 'alpha250-4';
    const project = board === 'alpha250-dpll' ? 'alpha250/dpll' : board + '/phase-noise-analyzer';
    const dom = new JSDOM(read(`examples/${project}/web/index.html`), {runScripts:'outside-only', pretendToBeVisual:true});
    const w = dom.window, d = w.document;
    w.setTimeout = w.requestAnimationFrame = () => 0;
    const adapter = signed ? 'examples/alpha250-4/phase-noise-analyzer/web/plot.ts' : 'web/phase-noise/analyzer/plot.ts';
    w.eval(ts.transpileModule([...common, adapter].map(read).join('\n'),
        {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText + '\nwindow.Plot = Plot; window.PnaReferences = PnaReferences;');
    const state = {traces:[], frames:0};
    const basics = {setLogX() {}, enableDecimation() {}, enableBatchedLines() {}, setPrimaryTraceLabel() {},
        setRangeX() {}, setLinY() {}, refreshLegend() {}, needsRedraw() { return false; },
        redraw(data, size, peak, label, done, ref, final, traces) { state.traces=traces; done(); }};
    const driver = {parameters:parameters(signed), values:new Float32Array(9).fill(signed ? -2 : 2),
        async getPhaseNoise() {state.frames++; return this.values.slice();}};
    d.getElementById('plot-controls').disabled = false; // Enabled by each host after connecting.
    const plot = new w.Plot(d, driver, basics);
    t.after(() => {plot.dispose(); w.close();});
    await new Promise(resolve => setImmediate(resolve));
    const update = async () => { plot._lastTick=-Infinity; await plot.updatePlot(); };
    return {w, d, plot, driver, state, update, signed};
}

for (const board of ['alpha250', 'alpha250-4', 'red-pitaya', 'alpha250-dpll']) {
    test(`${board}: actual page supports multiple named captures, visibility, recapture, undo and reusable files`, async t => {
        const {w,d,plot,driver,state,update,signed} = await host(t,board);
        assert.ok(d.querySelector('link[href="plot-references.css"]'));
        const capture = d.getElementById('capture-reference');
        capture.click();
        const first=plot.references.items[0], samples=Array.from(first.psd), cached=first.data;
        const name=d.querySelector('.reference-row input[type=text]');
        name.value='Before "filter", <b>reference</b>'; name.dispatchEvent(new w.Event('change'));
        assert.equal(first.name, name.value);
        assert.equal(d.querySelector('#reference-list b'),null);
        assert.ok(state.traces.some(trace => trace.label?.includes('&lt;b&gt;')));
        driver.parameters.fs=4000; driver.parameters.channel=1; driver.values.fill(signed ? -8 : 8);
        await update(); capture.click();
        const second=plot.references.items[1];
        assert.equal(d.getElementById('reference-count').textContent,'2 / 8');
        assert.equal(first.data[4][0],2000); assert.equal(second.data[4][0],1000);
        assert.deepEqual(Array.from(first.psd),samples); assert.strictEqual(first.data,cached);
        assert.notEqual(first.color,second.color);
        const rows=d.querySelectorAll('.reference-row');
        rows[0].querySelector('input[type=checkbox]').click();
        assert.equal(plot.visibleReferences.length,1);
        assert(!state.traces.some(trace => trace.label?.includes('&lt;b&gt;')));
        const exportAdapter = signed ? 'examples/alpha250-4/phase-noise-analyzer/web/export-file/export-file.ts'
            : 'web/phase-noise/analyzer/export-file/export-file.ts';
        w.eval(ts.transpileModule([ 'web/phase-noise/export-file/export-file.ts', exportAdapter ].map(read).join('\n'),
            {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText + '\nwindow.ExportFile = ExportFile;');
        w.Blob = Blob;
        const exporter = new w.ExportFile(d, plot); let exported;
        exporter.download = blob => {exported=blob;}; exporter.exportData();
        let csv = await exported.text();
        assert.equal(csv.split('Reference trace').length,2);
        assert(csv.includes('"Name","' + second.name + '"')); assert(!csv.includes('Before ""filter""'));
        rows[0].querySelector('input[type=checkbox]').click();
        exporter.exportData(); csv=await exported.text();
        assert.equal(csv.split('Reference trace').length,3);
        assert(csv.includes('"Name","Before ""filter"", <b>reference</b>"'));
        assert(csv.includes('"Sampling frequency (Hz)",8000'));
        assert(csv.includes('"Sampling frequency (Hz)",4000'));
        rows[0].querySelector('input[type=checkbox]').click();
        rows[0].querySelector('.replace-reference').click();
        assert.equal(plot.references.items[0].parameters.fs,4000);
        assert.equal(plot.references.items[0].name,first.name);
        assert.equal(plot.references.items[0].visible,false);
        d.getElementById('undo-reference').click(); assert.strictEqual(plot.references.items[0],first);
        const saved=plot.references.serialize();
        const imported=new w.PnaReferences(board,signed); imported.load(saved);
        assert.deepEqual(JSON.parse(imported.serialize()),JSON.parse(saved));
        assert.equal(imported.items[0].data,undefined);
        d.getElementById('clear-reference').click(); assert.equal(plot.references.items.length,0);
        d.getElementById('undo-reference').click(); assert.equal(plot.references.items.length,2);
        plot.references.clear(); plot.references.load(saved);
        assert.equal(plot.visibleReferences.length,1);
        // Unit changes use each capture's frequency grid, including signed CSD.
        plot.laserPlotType='frequency'; plot.updateReferenceDisplay();
        assert.equal(plot.references.items[0].data[4][1],10*Math.log10(Math.abs(samples[4])*2000**2));
        if (signed) assert.equal(plot.references.items[0].negative[2][0],2000);
        // No capture conversions or panel rebuilds occur on an ordinary frame.
        const row=d.querySelector('.reference-row'), data=plot.references.items[0].data;
        await update(); assert.strictEqual(d.querySelector('.reference-row'),row);
        assert.strictEqual(plot.references.items[0].data,data);
    });
}

test('PNA rejects incompatible and malformed files atomically, including signed data in an unsigned analyzer', async t => {
    const {w,plot} = await host(t,'alpha250'); plot.captureReference();
    const original=plot.references.serialize();
    for (const change of [
        f=>f.board='red-pitaya', f=>f.format='koheron-fft-references', f=>f.version=2,
        f=>f.references[0].psd.pop(), f=>f.references[0].psd[2]=-1, f=>f.references[0].psd[2]='2',
        f=>f.references[0].psd[2]=1e100, f=>f.references[0].parameters.fs=0,
        f=>f.references[0].parameters.channel=2, f=>f.references[0].parameters.analyzer_mode='invalid',
        f=>f.references[0].parameters.interferometer_delay=null, f=>f.references[0].name='',
        f=>f.references.push({...f.references[0],color:'red;position:fixed'})
    ]) {
        const invalid=JSON.parse(original); change(invalid);
        assert.throws(()=>plot.references.load(JSON.stringify(invalid)));
        assert.equal(plot.references.serialize(),original);
    }
    assert.throws(()=>plot.references.load('{'));
    const valid=JSON.parse(original); valid.references[0].psd[2]=null;
    const loaded=new w.PnaReferences('alpha250',false); loaded.load(JSON.stringify(valid));
    assert(Number.isNaN(loaded.items[0].psd[2]));
});

test('capture limit and unavailable frames disable acquisition actions without losing references or saved files', async t => {
    const {d,plot,driver,update} = await host(t,'alpha250-dpll');
    const capture=d.getElementById('capture-reference');
    for(let i=0;i<8;i++) capture.click();
    assert(capture.disabled); assert.equal(plot.references.items.length,8);
    assert.throws(()=>plot.references.load(plot.references.serialize()),/exceed 8/);
    assert.equal(d.querySelector('.replace-reference').disabled,false);
    const saved=plot.references.serialize();
    driver.values.fill(0); await update();
    assert(capture.disabled); assert(d.querySelector('.replace-reference').disabled);
    plot.replaceReference(plot.references.items[0]); plot.captureReference();
    assert.equal(plot.references.serialize(),saved);
    assert.equal(d.getElementById('save-references').disabled,false);
    d.querySelector('.remove-reference').click(); assert.equal(plot.references.items.length,7);
    assert(capture.disabled);
    driver.values.fill(2); await update(); assert.equal(capture.disabled,false);
});
