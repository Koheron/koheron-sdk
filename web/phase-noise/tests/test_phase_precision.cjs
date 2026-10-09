const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');

for (const board of ['alpha250', 'alpha250-4', 'red-pitaya']) {
  test(`${board}: precision choices, calibrated status and disposal`, async t => {
    const root = path.resolve(__dirname, '../../..');
    const dom = new JSDOM(fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/index.html`), 'utf8'), {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const w = dom.window, calls = [];
    let requested = 0, captured = 0, state = 1, overruns = 0, timer;
    w.setTimeout = callback => {timer = callback; return 42;};
    w.clearTimeout = id => calls.push(['cancel', id]);
    w.Command = (id, command, ...args) => [command, ...args];
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/phase-noise/phase-precision.ts'), 'utf8'),
      {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PhasePrecision = PhasePrecision;');
    const client = {
      getDriver() {return {id: 1, getCmds: () => ({set_phase_precision: 'set', get_precision_status: 'get', get_stream_status: 'stream'})};},
      async readTuple(command) {return command[0] === 'stream' ? [0,0,Math.floor(overruns / 4294967296),overruns % 4294967296,32768,16384,3] : [requested, captured, .0016 / 2**requested, state];},
      async readBool(command) {calls.push(command); requested = captured = command[1]; return true;}
    };
    const widget = new w.PhasePrecision(client, w.document);
    await widget.init();
    const select = w.document.getElementById('phase-precision'), status = w.document.getElementById('precision-status');
    assert.deepEqual([...select.options].map(option => +option.value), [0,1,2,3,4,5,6,7,8]);
    for (let bits = 0; bits <= 8; ++bits) {
      select.value = String(bits); select.dispatchEvent(new w.Event('change'));
      await new Promise(resolve => setImmediate(resolve));
      assert.deepEqual(calls.at(-1), ['set', bits]);
      assert.match(status.title, new RegExp(`Requested \\+${bits} bits; last packet \\+${bits} bits`));
      assert.equal(select.disabled, false);
    }
    overruns = 3; await timer();
    assert.match(status.textContent, /Live · Skips/);
    assert.match(status.title, /3 consumer overruns/);
    assert.match(status.title, /valid averages were retained/);
    overruns = 4294967299; await timer();
    assert.match(status.title, /4294967299 consumer overruns/);
    state = 2; await timer();
    assert.match(status.textContent, /Overrange/);
    assert.match(status.title, /Reduce precision/);
    state = 4; await timer();
    assert.match(status.textContent, /Sample gap/);
    assert.match(status.title, /ADC samples were lost/);
    assert.equal(status.dataset.state, 'error');
    widget.dispose();
    assert.deepEqual(calls.at(-1), ['cancel', 42]);
    const before = calls.length;
    select.dispatchEvent(new w.Event('change'));
    await new Promise(resolve => setImmediate(resolve));
    assert.equal(calls.length, before);
  });
}

for (const board of ['alpha250', 'alpha250-4', 'red-pitaya']) {
  test(`${board}: sample coverage, recent recovery, epochs and unavailable status`, async t => {
    const root = path.resolve(__dirname, '../../..');
    const dom = new JSDOM(fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/index.html`), 'utf8'), {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const w = dom.window;
    let now = 0, epoch = 1, covered = 100, span = 100, state = 1, timer, fail = false;
    w.performance.now = () => now;
    w.setTimeout = callback => {timer = callback; return 42;};
    w.clearTimeout = () => {};
    w.Command = (id, command, ...args) => [command, ...args];
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/phase-noise/phase-precision.ts'), 'utf8'),
      {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PhasePrecision = PhasePrecision;');
    const pair = value => [Math.floor(value / 4294967296), value % 4294967296];
    const commands = {set_phase_precision: 'set', get_precision_status: 'get', get_stream_coverage: 'coverage'};
    const client = {
      getDriver() {return {id: 1, getCmds: () => commands};},
      async readTuple(command) {
        if (fail) throw new Error('offline');
        return command[0] === 'coverage' ? [...pair(epoch), ...pair(covered), ...pair(span)] : [0,0,.0016,state];
      }
    };
    const widget = new w.PhasePrecision(client, w.document);
    await widget.init();
    const badge = w.document.getElementById('coverage-status');
    assert.equal(badge.textContent, 'Coverage 100%');
    assert.equal(badge.dataset.state, 'live');
    now = 1000; covered = 150; span = 300; await timer();
    assert.equal(badge.textContent, 'Coverage 50.0%');
    assert.equal(badge.dataset.state, 'warning');
    assert.match(badge.title, /50.0% skipped/);
    // Recent coverage recovers to 100% even though the lifetime total is 70%.
    now = 11000; covered = 350; span = 500; await timer();
    assert.equal(badge.textContent, 'Coverage 100%');
    assert.match(badge.title, /70.0% covered/);
    // A settings change clears old history, including high-word RPC epochs.
    epoch = 4294967301; covered = span = 0; now = 11500; await timer();
    assert.equal(badge.textContent, 'Coverage —');
    covered = span = 4294967302; now = 12000; await timer();
    assert.equal(badge.textContent, 'Coverage 100%');
    state = 4; await timer();
    assert.equal(badge.textContent, 'Coverage —');
    state = 1; fail = true; await timer();
    assert.equal(badge.textContent, 'Coverage —');
    assert.equal(badge.dataset.state, 'unknown');
    fail = false; delete commands.get_stream_coverage; await timer();
    assert.equal(badge.textContent, 'Coverage n/a');
    widget.dispose();
  });
}

for (const board of ['alpha250', 'alpha250-4', 'red-pitaya']) {
  test(`${board}: queue warning precedes coverage loss and reports processing capacity`, async t => {
    const root = path.resolve(__dirname, '../../..');
    const dom = new JSDOM(fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/index.html`), 'utf8'), {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const w = dom.window;
    let timer, state = 1, fail = false, performance = [10,7,1,1,1,800,2500,102,100];
    w.setTimeout = callback => {timer = callback; return 42;};
    w.clearTimeout = () => {};
    w.Command = (id, command, ...args) => [command, ...args];
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/phase-noise/phase-precision.ts'), 'utf8'),
      {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PhasePrecision = PhasePrecision;');
    const commands = {get_precision_status: 'precision', get_stream_performance: 'performance', get_fft_performance: 'fft'};
    const client = {
      getDriver: () => ({id: 1, getCmds: () => commands}),
      async readTuple(command) {
        if (fail) throw new Error('offline');
        if (command[0] === 'fft') return [1,2,3,4,.5];
        return command[0] === 'performance' ? performance : [0,0,.0016,state];
      }
    };
    const widget = new w.PhasePrecision(client, w.document);
    await widget.init();
    const badge = w.document.getElementById('performance-status');
    assert.equal(badge.hidden, false);
    assert.equal(badge.textContent, 'Queue 800 ms');
    assert.equal(badge.dataset.state, 'warning');
    assert.match(badge.title, /capacity: 100.0 windows\/s; required: 102.0/);
    assert.match(badge.title, /copy 1.00, FFT 7.00, averaging 1.00/);
    assert.match(badge.title, /window preparation 2.00, transform 3.00/);
    performance = [5,3,1,.5,.5,10,2500,102,200]; await timer();
    assert.equal(badge.dataset.state, 'live');
    performance[5] = 1600; await timer();
    assert.equal(badge.textContent, 'Queue 1.60 s');
    assert.equal(badge.dataset.state, 'warning');
    state = 2; await timer();
    assert.equal(badge.textContent, 'Queue —');
    state = 1; fail = true; await timer();
    assert.equal(badge.dataset.state, 'unknown');
    fail = false; performance[0] = NaN; await timer();
    assert.equal(badge.textContent, 'Queue —');
    delete commands.get_stream_performance; await timer();
    assert.equal(badge.hidden, true);
    widget.dispose();
  });
}
