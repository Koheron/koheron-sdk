const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const project = path.join(__dirname, '..');

function fixture(t) {
  const dom = new JSDOM('<body data-board="alpha250-4"><button id="capture-reference"></button><button id="clear-reference"></button><button id="fit-view"></button><div id="reference-info"><span id="reference-status"></span></div><table id="decade-values-table"></table><span id="refresh-rate"></span><div id="plot-placeholder"></div><input id="show-smoothed-trace" type="checkbox" checked><button class="export-data"></button><button class="export-plot"></button></body>', {runScripts: 'outside-only', pretendToBeVisual: true});
  t.after(() => dom.window.close());
  const w = dom.window;
  w.eval(ts.transpileModule(fs.readFileSync(path.resolve(__dirname, '../../../../web/phase-noise/plot.ts'), 'utf8'),
    {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PnaPlot = PnaPlot;');
  w.eval(ts.transpileModule(fs.readFileSync(path.join(project, 'web/plot.ts'), 'utf8'),
    {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.Plot = Plot;');
  w.setTimeout = () => 0; w.requestAnimationFrame = () => 0;
  const plot = Object.create(w.Plot.prototype);
  const p = {data_size: 1025, fs: 204800, channel: 2, cic_rate: 488,
    fft_navg: 8, avgxy_count: 64, fdds0: 1e7, fdds1: 1e7, fdds2: 1e7, fdds3: 1e7, clkIndex: '2'};
  const state = {fits: 0, redraw: null};
  Object.assign(plot, {document: w.document, n_pts: p.data_size, samplingFrequency: p.fs,
    plot_data: [], linear_plot_data: [], smooth_plot_data: [], phase_psd: new Float32Array(0),
    negative_plot_data: [], negative_smooth_data: [], reference_negative_data: [], reference_negative_smooth_data: [],
    laserPlotType: 'phase', showSmoothedInput: w.document.querySelector('input'),
    decadeValuesTable: w.document.querySelector('table'), _busy: false, _targetHz: 20, _lastTick: -Infinity,
    rateStarted: 0, displayedFrames: 0, receivedFrames: 0, lastTableUpdate: -Infinity, lastStarted: -Infinity,
    readMs: 0, processMs: 0, drawMs: 0, schedulerMs: 0,
    driver: {parameters: p, async getPhaseNoise() {return new Float32Array(1025).fill(-2);}},
    plotBasics: {setRangeX(a, b) {state.range = [a,b];}, setLinY() {state.fits++;},
      refreshLegend() {}, needsRedraw() {return false;}, redraw(...args) {state.redraw = args; args[4]();}}});
  plot.setFreqAxis();
  return {w, plot, state};
}

test('cross-spectrum magnitude retains negative sign and smooths signed values first', t => {
  const {plot} = fixture(t);
  const psd = new Float32Array(plot.n_pts).fill(2);
  psd[0] = psd[1] = 1e20;
  for (let i=55; i<=70; i++) psd[i] = -4;
  plot.computeDisplaySpectrum(psd, 2); plot.computeSmoothedPlot(2);
  assert.equal(plot.linear_plot_data[64][1], -2);
  assert.ok(Math.abs(plot.plot_data[64][1] - 10*Math.log10(2)) < 1e-9);
  assert.ok(plot.negative_plot_data.some(p => p[0] === 6400));
  const scale = 10**.05;
  const selected = [...psd].filter((v,i) => i>=2 && i>=64/scale && i<=64*scale);
  const mean = selected.reduce((sum,v) => sum+v/2,0)/selected.length;
  assert.ok(Math.abs(plot.smooth_plot_data[64][1] - 10*Math.log10(Math.abs(mean))) < 1e-9);
  assert.ok(plot.negative_smooth_data.some(p => p[0] === 6400));
  assert.ok(Number.isNaN(plot.plot_data[1][1]));
  plot.linear_plot_data.forEach(row => row[1] = -1);
  assert.ok(plot.getDecadeValues().every(row => Number.isNaN(row[1])));
});

test('negative-only XY data remains capturable with signed reference bins and its original axis', async t => {
  const {w, plot, state} = fixture(t);
  await plot.updatePlot();
  assert.ok(plot.frameStatus);
  assert.equal(w.document.getElementById('capture-reference').disabled, false);
  plot.captureReference();
  const reference = plot.referencePSD;
  assert.equal(reference[64], -2);
  const originalFrequency = plot.reference_data[64][0];
  plot.driver.parameters.fs /= 2;
  plot.driver.parameters.cic_rate *= 2;
  plot.driver.getPhaseNoise = async () => new Float32Array(1025).fill(8);
  plot._lastTick = -Infinity;
  await plot.updatePlot();
  assert.equal(reference[64], -2);
  assert.equal(plot.reference_data[64][0], originalFrequency);
  assert.notEqual(plot.plot_data[64][0], originalFrequency);
  assert.equal(plot.referenceParameters.channel, 2);
  assert.equal(state.fits, 1);
  const markers = state.redraw[7].filter(series => series.points?.show);
  assert.ok(markers.every(series => series.lines.show === false));
  plot.clearReference();
  assert.equal(plot.referencePSD, undefined);
});

test('cached PSDs do not count as new display frames, and stopped pages do not redraw', async t => {
  const {plot, state} = fixture(t);
  await plot.updatePlot();
  const first = plot.displayedFrames;
  plot._lastTick = -Infinity;
  await plot.updatePlot();
  assert.equal(plot.displayedFrames, first);
  plot.dispose();
  state.redraw = null;
  await plot.updatePlot();
  assert.equal(state.redraw, null);
});

test('CSV exports signed live/reference PSD, four LOs and captured metadata', async t => {
  const {w, plot} = fixture(t);
  await plot.updatePlot(); plot.captureReference();
  w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/phase-noise/export-file/export-file.ts'), 'utf8') + '\n' + fs.readFileSync(path.join(project, 'web/export-file/export-file.ts'),'utf8'),
    {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText+'\nwindow.ExportFile=ExportFile;');
  const exporter = new w.ExportFile(w.document, plot);
  let blob;
  exporter.download = value => {blob=value;};
  exporter.exportData();
  const csv = await new Promise(resolve => {const reader=new w.FileReader();reader.onload=()=>resolve(reader.result);reader.readAsText(blob);});
  assert.ok(csv.includes('Signed phase PSD (rad^2/Hz)'));
  assert.ok(csv.includes('"LO 3 frequency (Hz)",10000000'));
  assert.ok(csv.includes('"Cumulative XY segments",64'));
  assert.ok(csv.includes('Reference trace'));
  assert.ok(csv.split('\n').some(line=>line.endsWith(',-2')));
});

test('all four nominal LO fields commit in Hz, reject invalid integers and show cumulative progress', async t => {
  const html = fs.readFileSync(path.join(project,'web/index.html'),'utf8');
  const dom = new JSDOM(html,{runScripts:'outside-only',pretendToBeVisual:true});
  t.after(()=>dom.window.close());const w=dom.window;
  w.document.getElementById('dds-frequency').innerHTML = fs.readFileSync(path.join(project,'web/dds-frequency/dds-frequency.html'),'utf8').replace(/<\/?template[^>]*>/g,'');
  for (const file of [path.join(root,'web/phase-modulator/frequency-input.ts'),path.join(project,'web/phase-noise-analyzer-app.ts')])
    w.eval(ts.transpileModule(fs.readFileSync(file,'utf8'),{compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText+'\n'+(file.includes('analyzer-app')?'window.ControlApp=PhaseNoiseAnalyzerApp;':'window.NumberInput=NumberInput; window.FrequencyInput=FrequencyInput;'));
  const browserTimeout=w.setTimeout.bind(w);
  w.setTimeout=(callback,delay)=>delay===0?browserTimeout(callback,0):0;
  w.document.getElementById('instrument-controls').disabled=false;
  w.document.getElementById('settings-controls').disabled=false;
  const p={data_size:15001,channel:2,fs:100e6/133,cic_rate:133,fft_navg:8,clkIndex:'2'};
  const nominal=[10e6,10e6,10e6,10e6];const calls=[];
  const driver={async getParameters(){return p;},async getNominalFrequencies(){return nominal;},
    async getTrackingParameters(){return {tracking_enabled:true,effective_tracking_bandwidth:.1,tracking_correction_x:4,tracking_correction_y:5,tracking_locked:true};},
    async getAverageStatus(){return {count:42,target:0};},async getMeasurements(){return {phase_jitter:.001,time_jitter:1e-11,freq_lo:100,freq_hi:1e5,carrier_power:-3};},
    setLocalOscillator(channel,hz){calls.push([channel,hz]);nominal[channel]=hz;},setCicRate(value){calls.push(['cic',value]);p.cic_rate=value;}};
  const app = new w.ControlApp(w.document,driver);await app.init();await new Promise(resolve=>setImmediate(resolve));
  assert.equal(w.document.querySelector('.plot-navg-input').disabled,true);
  assert.equal(w.document.getElementById('average-status').textContent, '42');
  const input=w.document.querySelector('.dds-input3');
  input.dispatchEvent(new w.FocusEvent('focus'));input.value='10.000000001';input.dispatchEvent(new w.Event('input'));
  input.dispatchEvent(new w.KeyboardEvent('keydown',{key:'Enter',bubbles:true}));await new Promise(resolve=>setTimeout(resolve,20));
  assert.equal(calls.length,1);assert.equal(calls[0][0],3);assert.ok(Math.abs(calls[0][1]-10000000.001)<1e-8);
  const cic=w.document.querySelector('.cic-rate-input');cic.value='4.5';cic.dispatchEvent(new w.Event('input'));
  cic.dispatchEvent(new w.KeyboardEvent('keydown',{key:'Enter',bubbles:true}));await new Promise(resolve=>setImmediate(resolve));
  assert.equal(calls.length,1);
  app.setSampleRate(200e6);
  for (let channel=0;channel<4;++channel)
    assert.equal(w.document.querySelector('.dds-input'+channel).getAttribute('aria-valuemax'),'100000000');
  app.setSampleRate(250e6);
  for (let channel=0;channel<4;++channel)
    assert.equal(w.document.querySelector('.dds-input'+channel).getAttribute('aria-valuemax'),'125000000');
  app.dispose();
});

test('frequency-noise display and captured reference use each trace frequency axis', async t => {
  const {plot} = fixture(t);
  await plot.updatePlot(); plot.captureReference();
  plot.driver.parameters.fs /= 2;
  plot._lastTick = -Infinity;
  await plot.updatePlot();
  plot.laserPlotType = 'frequency';
  plot.computeDisplaySpectrum(plot.phase_psd, 2);
  plot.updateReferenceDisplay();
  const bin=64;
  const expected=10*Math.log10(2*plot.plot_data[bin][0]**2);
  assert.ok(Math.abs(plot.plot_data[bin][1]-expected)<1e-9);
  assert.ok(Math.abs(plot.reference_data[bin][1]-plot.plot_data[bin][1]-10*Math.log10(4))<1e-9);
  assert.equal(plot.referencePSD[bin],-2);
});

test('Y requires its own two LOs, XY requires all four, and hidden pages stop polling', async t => {
  const {w,plot}=fixture(t);
  const p=plot.driver.parameters;
  p.channel=1;p.fdds0=p.fdds1=0;
  let reads=0;
  plot.driver.getPhaseNoise=async()=>{reads++;return new Float32Array(1025).fill(2);};
  await plot.updatePlot();assert.equal(reads,1);
  p.channel=2;plot._lastTick=-Infinity;
  await plot.updatePlot();assert.equal(reads,1);assert.equal(plot.frameStatus,undefined);
  p.fdds0=p.fdds1=1e7;
  Object.defineProperty(w.document,'hidden',{value:true});plot._lastTick=-Infinity;
  await plot.updatePlot();assert.equal(reads,1);
});

test('settings read the explicit CIC rate at either sample clock and retain legacy decoding', async t => {
  const dom=new JSDOM('',{runScripts:'outside-only'}); t.after(()=>dom.window.close());
  const w=dom.window; w.Command=(_id,cmd)=>cmd;
  w.eval(ts.transpileModule(fs.readFileSync(path.join(project,'web/phase-noise-analyzer.ts'),'utf8'),
    {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText+'\nwindow.Analyzer=PhaseNoiseAnalyzer;');
  for (const [rate,supported] of [[250e6,true],[200e6,true],[200e6,false]]) {
    const client={getDriver(){return {id:1,getCmds(){return supported?{get_parameters:1,get_cic_rate:2}:{get_parameters:1};}};},
      async readTuple(){return [16385,rate/268,2,rate/268/16384,1,1e7,1e7,1e7,1e7,2,0];},
      async readUint32(){return 134;}};
    const parameters=await new w.Analyzer(client).getParameters();
    assert.equal(parameters.cic_rate,134);
    assert.equal(parameters.fs,rate/268);
  }
});
