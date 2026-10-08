const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../..');
function fixture(t, board) {
    const html = fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/index.html`), 'utf8');
    const dom = new JSDOM(html, {runScripts:'outside-only'});
    const w = dom.window, d = w.document, timers = new Map(), errors = [];
    let id = 0, calls = 0, fail = false, onError;
    w.setTimeout = (callback, delay) => {timers.set(++id, {callback, delay}); return id;};
    w.clearTimeout = key => timers.delete(key);
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/instrument/events.ts'), 'utf8') + '\n' + fs.readFileSync(path.join(root, 'web/phase-noise/save-config.ts'), 'utf8'),
        {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PnaSaveConfig = PnaSaveConfig;');
    const action = new w.PnaSaveConfig(d, () => {calls++; if(fail) throw new Error('Socket closed');},
        error => {errors.push(error); onError?.();});
    const button = d.querySelector('.save-cfg'), status = d.getElementById('save-config-status');
    for (const fieldset of d.querySelectorAll('fieldset')) fieldset.disabled = false;
    t.after(() => {action.dispose(); w.close();});
    const tick = () => {
        assert.equal(timers.size, 1);
        const [key, timer] = timers.entries().next().value;
        assert.equal(timer.delay, 2000); timers.delete(key); timer.callback();
    };
    return {action,button,status,timers,errors,tick,get calls(){return calls;},
        set fail(value){fail=value;},set onError(value){onError=value;}};
}
for (const board of ['alpha250','alpha250-4','red-pitaya']) {
    test(`${board} save reports requests and resets feedback after the latest click`, t => {
        const h = fixture(t, board);
        assert.equal(h.calls, 0);
        h.button.click();
        assert.equal(h.calls, 1); assert.equal(h.button.textContent, 'Save requested');
        assert.equal(h.status.textContent, 'Analyzer settings save requested.');
        const oldTimer = h.timers.keys().next().value;
        h.button.click(); assert.equal(h.calls, 2);
        assert(!h.timers.has(oldTimer));
        h.tick(); assert.equal(h.button.textContent, 'Save settings');
        assert.equal(h.status.textContent, 'Analyzer settings save requested.');
    });
    test(`${board} save failure reports the error and disposal removes commands and timers`, t => {
        const h = fixture(t, board); h.fail = true; h.button.click();
        assert.equal(h.calls, 1); assert.equal(h.errors.length, 1);
        assert.equal(h.button.textContent, 'Save failed');
        assert.equal(h.status.textContent, 'Unable to send the save request.');
        h.tick(); h.fail = false; h.button.click();
        assert.equal(h.button.textContent, 'Save requested');
        const stale = h.timers.values().next().value.callback;
        h.action.dispose(); h.action.dispose();
        assert.equal(h.timers.size, 0); h.button.click(); stale();
        assert.equal(h.calls, 2); assert.equal(h.button.textContent, 'Save requested');
    });
}
test('connection-error teardown during a save does not schedule another timer', t => {
    const h = fixture(t, 'alpha250-4'); h.onError = () => h.action.dispose();
    h.fail = true; h.button.click();
    assert.equal(h.errors.length, 1); assert.equal(h.timers.size, 0);
    h.button.click(); assert.equal(h.calls, 1);
});
