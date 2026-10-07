const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const settle = (ms = 25) => new Promise(resolve => setTimeout(resolve, ms));

async function host(t, options = {}) {
  const dom = new JSDOM(read('examples/alpha250/dpll/web/index.html'), {runScripts: 'outside-only', url: 'http://board/'});
  const {window} = dom;
  const {document} = window;
  t.after(() => { window.dispatchEvent(new window.Event('pagehide')); window.close(); });
  const writes = [];
  const reads = [];
  const state = {
    frequencies: [1e6, 2e6], gains: [[-8, 16], [0, -32], [64, 0], [0, 128]],
    integrators: [5, 10], routes: [6, 1], reference: 2, sampleRate: 250e6,
    paths: [0, 0], failed: false, exits: 0, pool: 0
  };
  let resolveInit;
  window.Command = (id, name, ...args) => ({id, name, args});
  window.Client = class {
    constructor(ip, pool) { state.pool = pool; }
    async init() {
      if (options.holdInit) { await new Promise(resolve => { resolveInit = resolve; }); }
      if (options.failInit) { throw new Error('init failed'); }
    }
    exit() { state.exits++; }
    getDriver(name) { return {id: name, getCmds: () => new Proxy({}, {get: (_, key) => key})}; }
    send({name, args}) {
      writes.push({name, args});
      const [channel, value] = args;
      if (name === 'set_dds_freq') { state.frequencies[channel] = value; state.paths[channel] &= ~1; }
      const index = ['set_p_gain', 'set_pi_gain', 'set_i2_gain', 'set_i3_gain'].indexOf(name);
      if (index >= 0) { state.gains[index][channel] = value; }
      if (name === 'set_integrator') {
        state.integrators[channel] = args[2] ? state.integrators[channel] | (1 << value) : state.integrators[channel] & ~(1 << value);
        if (value === 1 && !args[2]) { state.paths[channel] &= ~1; }
      }
      if (name === 'set_dac_output') { state.routes[channel] = value; }
      if (name === 'set_reference_clock') { state.reference = channel; }
    }
    async readTuple({name}, format) {
      reads.push(name);
      if (state.failed) { throw new Error('read failed'); }
      if (name === 'get_control_parameters') {
        assert.equal(format, 'ddiiiiiiiiII');
        return [...state.frequencies, ...state.gains.flat(), ...state.integrators];
      }
      assert.equal(name, 'get_dac_outputs'); assert.equal(format, 'II');
      return state.routes.slice();
    }
    async readFloat64Array({name}) {
      assert.equal(name, 'get_gain_values');
      if (state.failed) { throw new Error('read failed'); }
      return new Float64Array(state.gains.flat());
    }
    async readUint32Array({name}) {
      assert.equal(name, 'get_p_path_status');
      if (state.failed) { throw new Error('read failed'); }
      return new Uint32Array(state.paths);
    }
    async readInt32({name, args}) {
      if (name === 'set_p_mode') {
        writes.push({name, args});
        if (options.failPath) { return -3; }
        state.paths[args[0]] = args[1] ? 3 : 0;
        return 0;
      }
      assert.equal(name, 'set_geometric_gain');
      writes.push({name, args});
      const [channel, gain, sign, step] = args;
      if (options.failGain) { return -2; }
      const mantissas = [2048,2139,2233,2332,2435,2543,2656,2774,2896,3025,3158,3298,3444,3597,3756,3922];
      state.gains[gain][channel] = sign * mantissas[step % 16] / 2048 * 2 ** Math.floor(step / 16);
      return 0;
    }
    async readUint32({name}) {
      assert.equal(name, 'get_reference_clock');
      if (state.failed) { throw new Error('read failed'); }
      return state.reference;
    }
    async readFloat64({name}) {
      assert.equal(name, 'get_dac_sampling_freq');
      if (state.failed) { throw new Error('read failed'); }
      return state.sampleRate;
    }
  };
  window.console.error = () => {};
  for (const link of document.querySelectorAll('link[rel="import"]')) {
    const paths = {
      'reference-clock.html': 'examples/alpha250/dpll/web/clock-generator/reference-clock.html',
      'plot-basics.html': 'web/plot-basics/plot-basics.html',
      'export-file.html': 'examples/alpha250/phase-noise-analyzer/web/export-file/export-file.html'
    };
    link.import = new window.DOMParser().parseFromString(read(paths[link.getAttribute('href')]), 'text/html');
  }
  // These tests isolate the feedback controls. The actual monitor composition
  // and shared PNA plot are exercised by test_monitor_web.cjs.
  window.DpllMonitor = class { async init() {} dispose() {} };
  // Mount using the actual shared import implementation, with the real drivers and editors.
  const koheron = read('web/koheron.ts');
  const sources = koheron.slice(koheron.indexOf('class Imports {')) + '\n' + [
    'web/phase-modulator/frequency-input.ts', 'examples/alpha250/dpll/web/dpll.ts',
    'examples/alpha250/dpll/web/clock-generator/clock-generator.ts',
    'examples/alpha250/dpll/web/control.ts', 'examples/alpha250/dpll/web/app.ts'
  ].map(read).join('\n');
  window.eval(ts.transpileModule(sources, {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText);
  window.dispatchEvent(new window.Event('HTMLImportsLoaded'));
  await settle();
  const inputs = Array.from(document.querySelectorAll('.frequency-input'));
  const row = (channel = 0, status = 'p_gain') => document.querySelector(`.gain-row[data-channel="${channel}"][data-status="${status}"]`);
  const type = (input, value) => { input.value = value; input.dispatchEvent(new window.Event('input', {bubbles: true})); };
  const key = (input, key) => input.dispatchEvent(new window.KeyboardEvent('keydown', {key, bubbles: true}));
  return {window, document, state, writes, reads, inputs, row, type, key, resolveInit};
}

test('startup reads both loops, signed gains, routing and clock without writes', async t => {
  const h = await host(t);
  assert.equal(h.state.pool, 1);
  assert.equal(h.writes.length, 0);
  assert.equal(h.document.querySelector('#instrument-controls').disabled, false);
  assert.equal(h.document.querySelector('#connection-status').dataset.state, 'live');
  assert.equal(h.document.querySelector('#board-label').textContent, 'ALPHA250 · 250 MS/s');
  assert.equal(Number(h.inputs[1].value.replace(/\s/g, '')), 2);
  assert.equal(h.inputs[0].getAttribute('aria-valuemax'), '125000000');
  assert.equal(h.row().querySelector('.gain-input').value, '3');
  assert.equal(h.row().querySelector('[aria-pressed="true"]').value, '-1');
  assert.deepEqual(Array.from(h.document.querySelectorAll('.dac-output'), input => input.value), ['6', '1']);
  assert.deepEqual(Array.from(h.document.querySelectorAll('.integrator-switch'), input => input.checked), [true, false, true, false, false, true, false, true]);
  assert.equal(h.document.querySelector('.clkgen-input[value="2"]').checked, true);
});

test('frequency units, Nyquist boundary and selected-digit tuning address only the edited loop', async t => {
  const h = await host(t);
  h.type(h.inputs[1], '12.345 MHz'); h.key(h.inputs[1], 'Enter'); await settle();
  assert.deepEqual(h.writes, [{name: 'set_dds_freq', args: [1, 12345000]}]);
  const unit = h.document.querySelector('.frequency-unit[data-channel="1"]');
  unit.value = 'kHz'; unit.dispatchEvent(new h.window.Event('change')); await settle();
  assert.equal(h.writes.length, 1); // Unit-only changes are read-only.
  unit.value = 'MHz'; unit.dispatchEvent(new h.window.Event('change'));
  h.type(h.inputs[0], '125 MHz'); h.key(h.inputs[0], 'Enter'); await settle();
  assert.deepEqual(h.writes[1], {name: 'set_dds_freq', args: [0, 125e6]});
  h.inputs[1].focus(); h.key(h.inputs[1], 'ArrowUp'); await settle(120);
  assert.deepEqual(h.writes[2], {name: 'set_dds_freq', args: [1, 12345001]});
  h.type(h.inputs[1], '0.01 GHz'); h.key(h.inputs[1], 'Enter'); await settle();
  assert.deepEqual(h.writes[3], {name: 'set_dds_freq', args: [1, 10000000]});
  assert.equal(unit.value, 'GHz');
  assert.equal(Number(h.inputs[1].value.replace(/\s/g, '')), .01);
});

test('frequency drafts survive telemetry and changed sampling limits; invalid entries never write', async t => {
  const h = await host(t);
  h.type(h.inputs[0], 'unfinished');
  h.state.sampleRate = 200e6;
  await settle(280);
  assert.equal(h.inputs[0].value, 'unfinished');
  assert.equal(h.inputs[0].getAttribute('aria-valuemax'), '100000000');
  for (const value of ['', '-1', '101 MHz', 'NaN']) {
    h.type(h.inputs[0], value); h.key(h.inputs[0], 'Enter'); await settle();
    assert.equal(h.inputs[0].getAttribute('aria-invalid'), 'true');
  }
  assert.equal(h.writes.length, 0);
  h.key(h.inputs[0], 'Escape');
  assert.equal(Number(h.inputs[0].value.replace(/\s/g, '')), 1);
  assert.equal(h.inputs[0].hasAttribute('aria-invalid'), false);
});

test('gain drafts stay with their channel, zero is exact, and invalid signed gains are rejected', async t => {
  const h = await host(t);
  const row = h.row(1);
  const input = row.querySelector('.gain-input');
  const apply = row.querySelector('.gain-save');
  h.type(input, '5'); await settle(280);
  assert.equal(input.value, '5');
  assert.equal(h.row(0).querySelector('.gain-input').value, '3');
  apply.click(); await settle();
  assert.deepEqual(h.writes[0], {name: 'set_geometric_gain', args: [1, 0, 1, 80]});
  row.querySelector('.gain-button[value="0"]').click(); apply.click(); await settle();
  assert.deepEqual(h.writes[1], {name: 'set_geometric_gain', args: [1, 0, 0, 0]});
  row.querySelector('.gain-button[value="1"]').click();
  for (const value of ['', '-1', '31', '32']) {
    h.type(input, value); apply.click(); await settle();
    assert.equal(input.checkValidity(), false);
  }
  assert.equal(h.writes.length, 2);
  row.querySelector('.gain-button[value="-1"]').click();
  h.type(input, '31'); apply.click(); await settle();
  assert.deepEqual(h.writes[2], {name: 'set_geometric_gain', args: [1, 0, -1, 496]});
  h.type(input, '7'); h.key(input, 'Escape'); await settle();
  assert.equal(input.value, '31');
});

test('integrator, DAC route and clock changes keep their native command values', async t => {
  const h = await host(t);
  const integrator = h.document.querySelector('.integrator-switch[data-channel="1"][data-integratorindex="0"]');
  integrator.checked = true; integrator.dispatchEvent(new h.window.Event('change'));
  const route = h.document.querySelector('.dac-output[data-channel="0"]');
  route.value = '3'; route.dispatchEvent(new h.window.Event('change'));
  const reference = h.document.querySelector('.clkgen-input[value="0"]');
  reference.checked = true; reference.dispatchEvent(new h.window.Event('change'));
  assert.deepEqual(h.writes, [
    {name: 'set_integrator', args: [1, 0, true]},
    {name: 'set_dac_output', args: [0, 3]},
    {name: 'set_reference_clock', args: [0]}
  ]);
});

test('page exit cancels queued edits, removes handlers and stops polling', async t => {
  const h = await host(t);
  h.inputs[0].focus(); h.key(h.inputs[0], 'ArrowUp');
  h.window.dispatchEvent(new h.window.Event('pagehide'));
  const readCount = h.reads.length;
  await settle(300);
  assert.equal(h.writes.length, 0);
  assert.equal(h.reads.length, readCount);
  assert.equal(h.state.exits, 1);
  assert.equal(h.document.querySelector('#instrument-controls').disabled, true);
  h.window.dispatchEvent(new h.window.Event('beforeunload'));
  assert.equal(h.state.exits, 1);
});

test('failed connection disables controls and exposes retry without startup writes', async t => {
  const h = await host(t, {failInit: true});
  assert.equal(h.document.querySelector('#instrument-controls').disabled, true);
  assert.equal(h.document.querySelector('#connection-status').dataset.state, 'error');
  assert.equal(h.document.querySelector('#connection-error').hidden, false);
  assert.equal(h.writes.length, 0);
  assert.equal(h.state.exits, 1);
});

test('poll failure stops acquisition and cancels editors', async t => {
  const h = await host(t);
  h.state.failed = true;
  await settle(280);
  assert.equal(h.document.querySelector('#connection-status').textContent, 'Disconnected');
  assert.equal(h.document.querySelector('#instrument-controls').disabled, true);
  const count = h.reads.length;
  await settle(280);
  assert.equal(h.reads.length, count);
  assert.equal(h.state.exits, 1);
});

test('late initialization after page exit cannot enable controls or start polling', async t => {
  const h = await host(t, {holdInit: true});
  h.window.dispatchEvent(new h.window.Event('pagehide'));
  h.resolveInit(); await settle();
  assert.equal(h.document.querySelector('#instrument-controls').disabled, true);
  assert.equal(h.reads.length, 0);
  assert.equal(h.state.exits, 1);
});


test('sixteenth-octave gains retain fractional precision in their applied readback', async t => {
  const h = await host(t);
  const row = h.row(0);
  row.querySelector('.gain-button[value="1"]').click();
  h.type(row.querySelector('.gain-input'), '0.0625');
  row.querySelector('.gain-save').click();
  await settle();
  assert.deepEqual(h.writes, [{name: 'set_geometric_gain', args: [0, 0, 1, 1]}]);
  assert.equal(row.querySelector('.gain-input').value, '0.0625');
  assert.equal(row.querySelector('.gain-value').value, '1.04443359375');
  h.type(row.querySelector('.gain-input'), '0.01');
  row.querySelector('.gain-save').click();
  await settle();
  assert.equal(h.writes.length, 1);
});

test('hardware rejection of a gain update disables controls instead of claiming success', async t => {
  const h = await host(t, {failGain: true});
  h.type(h.row().querySelector('.gain-input'), '4');
  h.row().querySelector('.gain-save').click();
  await settle();
  assert.equal(h.document.querySelector('#connection-status').dataset.state, 'error');
  assert.equal(h.document.querySelector('#instrument-controls').disabled, true);
});


test('manual P mode is read-only at startup, isolated per channel, and uses acknowledged updates', async t => {
  const h = await host(t);
  const selects = h.document.querySelectorAll('.p-mode');
  assert.deepEqual(Array.from(selects, s => s.value), ['0', '0']);
  assert.equal(h.writes.length, 0);
  selects[1].value = '1'; selects[1].dispatchEvent(new h.window.Event('change'));
  assert.equal(selects[1].disabled, true);
  await settle();
  assert.deepEqual(h.writes, [{name: 'set_p_mode', args: [1, 1]}]);
  assert.equal(selects[0].value, '0'); assert.equal(selects[1].value, '1');
  assert.equal(selects[1].disabled, false);
  h.state.paths[1] = 1; await settle(280);
  assert.match(h.document.querySelector('.p-mode-status[data-channel="1"]').textContent, /outside estimate range/);
  assert.equal(h.writes.length, 1); // Range telemetry never switches automatically.
  selects[1].value = '0'; selects[1].dispatchEvent(new h.window.Event('change')); await settle();
  assert.deepEqual(h.writes[1], {name: 'set_p_mode', args: [1, 0]});
  assert.equal(selects[1].value, '0');
});

test('rejected manual P calibration retains Accurate mode and allows recovery', async t => {
  const h = await host(t, {failPath: true});
  const select = h.document.querySelector('.p-mode');
  select.value = '1'; select.dispatchEvent(new h.window.Event('change')); await settle();
  assert.equal(h.document.querySelector('#instrument-controls').disabled, false);
  assert.equal(h.document.querySelector('#connection-status').dataset.state, 'live');
  assert.equal(select.value, '0'); assert.equal(select.disabled, false);
  assert.match(h.document.querySelector('.p-mode-status').textContent, /stable signal/);
});
