const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');

for (const board of ['alpha250', 'alpha250-4', 'red-pitaya']) {
  test(`${board}: precision choices, calibrated status and disposal`, async t => {
    const root = path.resolve(__dirname, '../../..');
    const dom = new JSDOM(fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/index.html`), 'utf8'), {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const w = dom.window, calls = [];
    let requested = 0, captured = 0, state = 1, timer;
    w.setTimeout = callback => {timer = callback; return 42;};
    w.clearTimeout = id => calls.push(['cancel', id]);
    w.Command = (id, command, ...args) => [command, ...args];
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/phase-noise/phase-precision.ts'), 'utf8'),
      {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PhasePrecision = PhasePrecision;');
    const client = {
      getDriver() {return {id: 1, getCmds: () => ({set_phase_precision: 'set', get_precision_status: 'get'})};},
      async readTuple() {return [requested, captured, .0016 / 2**requested, state];},
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
