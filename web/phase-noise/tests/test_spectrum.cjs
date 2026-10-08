const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../..');

function fixture(t) {
  const dom = new JSDOM('<button id="capture-reference"></button><button id="clear-reference"></button><div id="reference-info"><span id="reference-status"></span></div><table id="decade-values-table"></table>', {runScripts: 'outside-only', pretendToBeVisual: true});
  t.after(() => dom.window.close());
  const w = dom.window;
  for (const [file, names] of [
    ['web/koheron.ts', ['Client']], ['web/phase-noise/spectrum.ts', ['readPnaSpectrum']],
    ['web/phase-noise/plot.ts', ['PnaPlot']],
    ['web/phase-noise/analyzer/plot.ts', ['Plot']]
  ]) {
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'),
      {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText
      + names.map(name => `\nwindow.${name} = ${name};`).join(''));
  }
  return w;
}

test('browser decodes the production C++ spectrum frame including signed bins', async t => {
  const w = fixture(t);
  const wire = fs.readFileSync(path.join(root, 'tmp/tests/phase-noise/spectrum-frame.bin'));
  const bytes = new w.Uint8Array(wire);
  const client = Object.create(w.Client.prototype);
  client._readBaseAsync = async () => new w.DataView(bytes.buffer, 8);
  const frame = await w.readPnaSpectrum(client, {});
  assert.equal(frame.metadata.sequence, 1);
  assert.equal(frame.metadata.state, 1);
  assert.equal(frame.metadata.precision, 8);
  assert.equal(frame.metadata.fs, 1e6);
  assert.equal(frame.metadata.fdds0, 10e6 + .125);
  assert.equal(frame.metadata.fdds2, 20e6 + .25);
  assert.equal(frame.metadata.avgxy_count, 7);
  assert.deepEqual(Array.from(frame.values), [0, 0, -2, 4, 8]);
});

test('malformed spectrum framing and inexact sequence numbers are rejected', async t => {
  const w = fixture(t);
  const client = Object.create(w.Client.prototype);
  let wire = new w.DataView(new w.ArrayBuffer(96));
  client._readBaseAsync = async () => wire;
  wire.setUint32(92, 3);
  await assert.rejects(w.readPnaSpectrum(client, {}), /Invalid spectrum vector length/);
  wire = new w.DataView(new w.ArrayBuffer(91));
  await assert.rejects(w.readPnaSpectrum(client, {}), /Truncated spectrum metadata/);
  wire = new w.DataView(new w.ArrayBuffer(96));
  wire.setUint32(0, 0x200000);
  await assert.rejects(w.readPnaSpectrum(client, {}), /integer precision/);
});

test('plot uses captured settings and sequence even when live controls disagree', async t => {
  const w = fixture(t);
  const plot = Object.create(w.Plot.prototype);
  w.setTimeout = w.requestAnimationFrame = () => 0;
  const live = {data_size: 5, fs: 10e6, channel: 0, cic_rate: 10, fft_navg: 1, fdds0: 10e6, fdds1: 20e6, analyzer_mode: 'rf'};
  const captured = {...live, fs: 1e6, channel: 1, cic_rate: 100};
  let sequence = 3;
  Object.assign(plot, {document: w.document, driver: {parameters: live,
    async getSpectrumSnapshot() {return {sequence, state: 1, parameters: captured, values: new Float32Array([0,0,2,4,8])};}},
    n_pts: 5, samplingFrequency: live.fs, plot_data: [], laserPlotType: 'phase',
    decadeValuesTable: w.document.querySelector('table'), _busy: false, _targetHz: 60, _lastTick: -Infinity,
    rateStarted: w.performance.now(), displayedFrames: 0, receivedFrames: 0,
    readMs: 0, processMs: 0, drawMs: 0, schedulerMs: 0, lastTableUpdate: -Infinity,
    plotBasics: {setRangeX() {}, setLinY() {}, refreshLegend() {}, needsRedraw() {return false;}, redraw(...args) {args[4]();}}});
  await plot.updatePlot();
  assert.equal(plot.plot_data[2][0], 250000);
  assert.equal(plot.frameStatus.channel, 1);
  plot.captureReference();
  assert.equal(plot.referenceParameters.fs, 1e6);
  const count = plot.displayedFrames;
  plot._lastTick = -Infinity;
  await plot.updatePlot();
  assert.equal(plot.displayedFrames, count);
  sequence++;
  plot._lastTick = -Infinity;
  await plot.updatePlot();
  assert.equal(plot.displayedFrames, count + 1, 'new acquisitions can have identical PSD values');
});
