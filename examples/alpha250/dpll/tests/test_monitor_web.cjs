const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const install = require('./fixtures/monitor-client.cjs');
const root = path.resolve(__dirname, '../../../..');
const read = p => fs.readFileSync(path.join(root, p), 'utf8');
const settle = () => new Promise(resolve => setTimeout(resolve, 30));
async function host(t) {
  const dom = new JSDOM(read('examples/alpha250/dpll/web/index.html'), {runScripts: 'outside-only', pretendToBeVisual: true});
  const w = dom.window, d = w.document, state = install(w);
  w.Client = w.MockClient; w.Command = w.MockCommand;
  w.$ = () => ({});
  w.PlotBasics = class {
    setLogX() {} enableDecimation() {} enableBatchedLines() {} setPrimaryTraceLabel() {}
    setRangeX() {} setLinY() {} refreshLegend() {} needsRedraw() { return false; }
    redraw(data, size, peak, label, complete) { complete(); }
  };
  for (const [id, p] of [
    ['plot-basics', 'web/plot-basics/plot-basics.html'],
    ['export-file', 'web/phase-noise/export-file/export-file.html']]) {
    const template = new w.DOMParser().parseFromString(read(p), 'text/html').querySelector('template');
    d.getElementById(id).appendChild(d.importNode(template.content, true));
  }
  const files = ['web/phase-modulator/frequency-input.ts', 'web/phase-noise/spectrum.ts',
    'web/phase-noise/plot.ts', 'web/phase-noise/phase-precision.ts',
    'web/phase-noise/analyzer/phase-noise-analyzer.ts',
    'web/phase-noise/analyzer/plot.ts',
    'web/phase-noise/export-file/export-file.ts',
    'web/phase-noise/analyzer/export-file/export-file.ts',
    'examples/alpha250/dpll/web/monitor.ts'];
  w.eval(ts.transpileModule(files.map(read).join('\n'), {compilerOptions: {target: ts.ScriptTarget.ES5}}).outputText + '\nwindow.DpllMonitor = DpllMonitor;');
  const errors = [];
  const monitor = new w.DpllMonitor(d, 'board', error => { errors.push(error); monitor.dispose(); });
  t.after(() => { monitor.dispose(); w.close(); });
  await monitor.init(); await settle();
  return {w, d, monitor, errors, state};
}

test('monitor uses the shared PNA frame, precision and controls without loop writes on startup', async t => {
  const {d, state, monitor, errors} = await host(t);
  assert.deepEqual(errors, []);
  assert.deepEqual(state.writes, []);
  assert.equal(d.getElementById('monitor-controls').disabled, false);
  assert.equal(d.getElementById('average-status').textContent, '4/');
  assert.equal(d.querySelector('.phase-jitter-span').textContent, '12.34 mrad');
  assert.equal(monitor.plot.phase_psd.length, 16385);
  assert.equal(monitor.plot.frameStatus.fs, 6250000);
  assert.equal(d.getElementById('coverage-status').textContent, 'Coverage 100%');
  assert.equal(d.getElementById('performance-status').hidden, false);
  assert.ok(state.calls.every(call => call.id === 'Dma'));
  d.getElementById('capture-reference').click();
  assert.equal(monitor.plot.referenceParameters.channel, 0);
});

test('channel, decimation, averages and precision target the monitor only; reference stays on its captured grid', async t => {
  const {d, w, state, monitor, errors} = await host(t);
  d.getElementById('capture-reference').click();
  d.querySelector('[name="monitor-channel"][value="1"]').click();
  for (const [id, value] of [['monitor-decimation', '32'], ['monitor-averages', '8']]) {
    const input = d.getElementById(id);
    input.value = value;
    input.dispatchEvent(new w.Event('input', {bubbles: true}));
    input.dispatchEvent(new w.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    await settle();
  }
  const precision = d.getElementById('phase-precision');
  precision.value = '4'; precision.dispatchEvent(new w.Event('change'));
  await settle();
  d.getElementById('reset-average').click();
  await settle();
  assert.deepEqual(errors, []);
  assert.ok(state.writes.every(call => call.id === 'Dma'));
  assert.deepEqual(state.writes.map(call => call.name), ['set_channel', 'set_cic_rate', 'set_fft_navg', 'set_phase_precision', 'reset_average']);
  assert.equal(monitor.plot.referenceParameters.channel, 0);
  assert.equal(monitor.plot.referenceParameters.fs, 6250000);
  assert.equal(monitor.plot.frameStatus.channel, 1);
  assert.equal(monitor.plot.frameStatus.fs, 3906250);
});

test('disconnect and disposal stop reads, clear readouts and disable monitor controls', async t => {
  const {d, state, monitor, errors} = await host(t);
  state.failed = true;
  await monitor.poll();
  assert.equal(errors.length, 1);
  assert.equal(d.getElementById('monitor-controls').disabled, true);
  assert.equal(d.querySelector('.phase-jitter-span').textContent, '—');
  const count = state.calls.length;
  await settle();
  assert.equal(state.calls.length, count);
  assert.equal(state.closed, 1);
});

test('monitor decimation rejects odd entry and tunes by two without loop writes', async t => {
  const {d, w, state} = await host(t);
  const input = d.getElementById('monitor-decimation');
  input.focus();
  input.value = '21';
  input.dispatchEvent(new w.Event('input', {bubbles: true}));
  input.dispatchEvent(new w.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
  await settle();
  assert.deepEqual(state.writes, []);
  assert.equal(input.getAttribute('aria-invalid'), 'true');
  input.dispatchEvent(new w.KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
  input.dispatchEvent(new w.KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true}));
  await settle();
  assert.deepEqual(state.writes.map(call => [call.id, call.name, call.args[0]]), [['Dma', 'set_cic_rate', 22]]);
});


test('live zero spectrum explains precision limit and recovers when noise is resolved', async t => {
  const {d, state} = await host(t);
  state.zeroSpectrum = true; state.sequence++;
  await settle();
  const status = d.getElementById('spectrum-status');
  assert.ok(status && !status.hidden);
  assert.match(status.textContent, /Increase phase precision/);
  assert.equal(d.getElementById('capture-reference').disabled, true);
  state.zeroSpectrum = false; state.sequence++;
  await settle();
  assert.equal(status.hidden, true);
  assert.equal(d.getElementById('capture-reference').disabled, false);
});
