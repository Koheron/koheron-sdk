const assert = require('node:assert/strict');
const fs = require('node:fs');
const test = require('node:test');
const {JSDOM} = require('jsdom');
const ts = require('../../transpile.cjs');

function host(t) {
    const dom = new JSDOM('<body data-board="alpha250"><input class="peak-input"></body>',
        {runScripts:'outside-only', pretendToBeVisual:true});
    const w = dom.window, d = w.document;
    for (const file of ['web/fft/workspace.html', 'web/fft/plot/yunit.html', 'web/fft/export-file/export-file.html']) {
        const container = d.createElement('div'); container.innerHTML = fs.readFileSync(file,'utf8');
        d.body.append(container.querySelector('template').content.cloneNode(true));
    }
    d.getElementById('history-canvas').remove();
    w.$ = () => ({on() {}, off() {}});
    let paint, frameTime=0;
    w.requestAnimationFrame = fn => {paint = fn; return 1;};
    w.cancelAnimationFrame = () => {paint = undefined;};
    w.setTimeout = () => 1; w.clearTimeout = () => {};
    const files = ['web/plot-references/references.ts','web/plot-references/panel.ts',
        'web/fft/plot/references.ts','web/fft/plot/spectrum-history.ts','web/fft/plot/plot.ts',
        'web/fft/export-file/export-file.ts'];
    w.eval(ts.transpileModule(files.map(f => fs.readFileSync(f,'utf8')).join('\n'),
        {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText +
        '\nwindow.Plot=Plot; window.ExportFile=ExportFile;');
    const status = {fs:250e6,channel:0,window_index:1,W1:.25,W2:.375,clkIndex:'2',
        dds_freq:[0,0],powerBasis:'full-scale'};
    const psd = new w.Float32Array(4).fill(1e-9);
    const driver = {fft_size:8,status,readSpectrum:async () => ({psd,status})};
    const basics = {enableSpectrumReduction(){},enableBatchedLines(){},setLinY(){},
        setRangeX(){}, getRangeX(){return {from:0,to:125};},needsRedraw(){return false;},redraw(){}};
    const plot = new w.Plot(d,driver,basics);
    t.after(() => {plot.dispose(); w.close();});
    const flush = async () => {
        for(let i=0;i<12;i++) await Promise.resolve();
        const fn=paint; paint=undefined; if(fn) fn(frameTime+=17);
    };
    return {w,d,plot,status,driver,flush};
}

test('blank calibration produces a labelled live spectrum with full-scale units only', async t => {
    const {w,d,plot,flush} = host(t); await flush();
    assert.equal(plot.unit,'dBFS-Hz'); assert.equal(plot.yLabel,'PSD (dBFS/Hz)');
    assert.equal(d.getElementById('connection-status').textContent,'Live spectrum');
    assert.equal(d.getElementById('fft-calibration-status').hidden,false);
    assert(d.querySelector('[value="dBFS-Hz"]').checked);
    for(const value of ['dBm-Hz','dBm','nv-rtHz']) {
        assert(d.querySelector(`[value="${value}"]`).disabled);
        assert(d.querySelector(`[value="${value}"]`).closest('label').hidden);
    }
    assert(Math.abs(plot.plot_data[1][1] + 90) < 1e-5);
    const unit = d.querySelector('[value="dBFS"]'); unit.checked=true;
    unit.dispatchEvent(new w.Event('change'));
    assert.equal(plot.yLabel,'Power (dBFS)');
    assert(Math.abs(plot.plot_data[1][1] - 10*Math.log10(1e-9*1.5*250e6/8)) < 1e-5);
    assert.match(d.getElementById('peak-level').textContent,/dBFS$/);
    const exporter = new w.ExportFile(d,plot,'ALPHA250');
    assert(exporter.frameRows(plot.frameStatus).some(row => /Unavail.*full-scale/.test(row)));
    assert.match(exporter.frameLabel(plot.frameStatus),/Uncalibrated/);
});

test('calibrated frames restore physical units without mixing history or references', async t => {
    const {d,plot,status,driver,flush} = host(t); await flush();
    plot.captureReference();
    const saved=plot.references.serialize();
    const refs=new plot.references.constructor('alpha250'); refs.load(saved);
    assert.equal(refs.items[0].status.powerBasis,'full-scale');
    const bad=JSON.parse(saved); bad.references[0].status.powerBasis='watts';
    assert.throws(() => refs.load(JSON.stringify(bad)));
    const epoch=plot.history.epoch;
    driver.readSpectrum=async () => ({psd:new Float32Array(4).fill(1e-12),status:{...status,powerBasis:undefined}});
    await plot.updatePlot(); await flush();
    assert.equal(plot.unit,'dBm-Hz'); assert.equal(plot.yLabel,'PSD (dBm/Hz)');
    assert.equal(d.getElementById('fft-calibration-status').hidden,true);
    assert.equal(d.querySelector('[value="dBm-Hz"]').disabled,false);
    assert.equal(plot.visibleReferences.length,0);
    assert(plot.history.epoch > epoch); assert.equal(plot.history.samples,1);
    assert(Number.isNaN(plot.convertValue(1e-9,'dBm-Hz',status)));
    assert(Number.isNaN(plot.convertValue(1e-9,'dBFS',plot.frameStatus)));
    assert(Math.abs(plot.plot_data[1][1] + 90) < 1e-5);
});
