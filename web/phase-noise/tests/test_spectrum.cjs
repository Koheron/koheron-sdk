const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../..');

function fixture(t) {
  const dom = new JSDOM('<button id="capture-reference"></button><div id="plot-references"></div><table id="decade-values-table"></table>', {runScripts: 'outside-only', pretendToBeVisual: true});
  t.after(() => dom.window.close());
  const w = dom.window;
  for (const [file, names] of [
    ['web/koheron.ts', ['Client']], ['web/phase-noise/spectrum.ts', ['readPnaSpectrum', 'PnaSpectrumReader']],
    ['web/plot-references/references.ts', ['PlotReferences']], ['web/plot-references/panel.ts', ['PlotReferencePanel']],
    ['web/phase-noise/references.ts', ['PnaReferences']], ['web/phase-noise/plot.ts', ['PnaPlot']],
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

test('browser decodes the compact C++ reply with current metadata and no density', async t => {
  const w = fixture(t);
  const wire = fs.readFileSync(path.join(root, 'tmp/tests/phase-noise/spectrum-frame.bin.cached'));
  const bytes = new w.Uint8Array(wire);
  const client = Object.create(w.Client.prototype);
  client._readBaseAsync = async () => new w.DataView(bytes.buffer, 8);
  const frame = await w.readPnaSpectrum(client, {});
  assert.equal(frame.metadata.sequence, 1);
  assert.equal(frame.metadata.state, 1);
  assert.equal(frame.values.length, 0);
});

test('conditional reads reuse density but keep current metadata and recover after a bad reply', async t => {
  const w = fixture(t);
  let now = 0;
  Object.defineProperty(w.performance, 'now', {value: () => now});
  const commands = [];
  const start = 0x100000005;
  const response = (sequence, state, values) => ({
    metadata: [sequence, state, 8, 1e6, 2, 100, 3, 7, 0, 10e6, 10e6, 20e6, 20e6, 0, 0, 2],
    values: new w.Float32Array(values)
  });
  const responses = [response(start, 1, [0, 0, 2, 4, 8]), response(start, 0, []),
    response(start + 1, 1, []), response(start + 1, 1, [0, 0, 3, 5, 9])];
  const client = {async readTupleWithFloat32Vector(command) {
    commands.push(command);
    return responses.shift();
  }};
  const cmds = {
    get_spectrum_snapshot: {id: 7, args: []},
    get_spectrum_snapshot_since: {id: 8, args: [{type: 'uint32_t'}, {type: 'uint32_t'}]}
  };
  const reader = new w.PnaSpectrumReader(client, 5, cmds);
  const first = await reader.read();
  assert.equal(first.transferredDensityBytes, 20);
  assert.equal(first.metadata.data_size, 5);
  const cached = await reader.read();
  assert.equal(cached.metadata.state, 0, 'state changes remain visible without a new density');
  assert.equal(cached.metadata.data_size, 5);
  assert.strictEqual(cached.values, first.values);
  assert.equal(cached.transferredDensityBytes, 0);
  assert.deepEqual(Array.from(commands[1].data.slice(8)), [0, 0, 0, 1, 0, 0, 0, 5]);
  await assert.rejects(reader.read(), /Changed spectrum has no density/);
  const refreshed = await reader.read();
  assert.deepEqual(Array.from(refreshed.values), [0, 0, 3, 5, 9]);
  assert.equal(commands[3].cmd.id, 7, 'a failed conditional read forces a full refresh');
  now = 6000;
  responses.push(response(start + 1, 1, [0, 0, 4, 6, 10]));
  const restarted = await reader.read();
  assert.equal(commands[4].cmd.id, 7, 'periodic full reads recover a reused sequence after board restart');
  assert.equal(restarted.values[2], 4);
});

test('an older V1 instrument without the conditional RPC keeps full snapshot reads', async t => {
  const w = fixture(t);
  const commands = [];
  const client = {async readTupleWithFloat32Vector(command) {
    commands.push(command.cmd.id);
    return {metadata: [commands.length, 1, 0, 1e6, 0, 20, 1, 1, 1,
      10e6, 10e6, 0, 0, 0, 0, 0], values: new w.Float32Array([0, 0, commands.length])};
  }};
  const reader = new w.PnaSpectrumReader(client, 5, {get_spectrum_snapshot: {id: 7, args: []}});
  assert.equal((await reader.read()).values[2], 1);
  assert.equal((await reader.read()).values[2], 2);
  assert.deepEqual(commands, [7, 7]);
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
  plot.initReferences();
  await plot.updatePlot();
  assert.equal(plot.plot_data[2][0], 250000);
  assert.equal(plot.frameStatus.channel, 1);
  plot.captureReference();
  assert.equal(plot.references.items[0].parameters.fs, 1e6);
  const count = plot.displayedFrames;
  plot._lastTick = -Infinity;
  await plot.updatePlot();
  assert.equal(plot.displayedFrames, count);
  sequence++;
  plot._lastTick = -Infinity;
  await plot.updatePlot();
  assert.equal(plot.displayedFrames, count + 1, 'new acquisitions can have identical PSD values');
});
