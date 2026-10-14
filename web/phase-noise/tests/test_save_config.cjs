const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../..');
function fixture(t, board) {
    const html = fs.readFileSync(path.join(root, `examples/${board}/phase-noise-analyzer/web/index.html`), 'utf8');
    const dom = new JSDOM(html, {runScripts:'outside-only'});
    const w = dom.window, d = w.document, timers = new Map(), errors = [];
    let id = 0, calls = 0, fail = false, result, onError;
    w.setTimeout = (callback, delay) => {timers.set(++id, {callback, delay}); return id;};
    w.clearTimeout = key => timers.delete(key);
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/instrument/events.ts'), 'utf8') + '\n' + fs.readFileSync(path.join(root, 'web/phase-noise/save-config.ts'), 'utf8'),
        {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PnaSaveConfig = PnaSaveConfig;');
    const action = new w.PnaSaveConfig(d, () => {calls++; if(fail) throw new Error('Socket closed'); return result;},
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
        set fail(value){fail=value;},set result(value){result=value;},set onError(value){onError=value;}};
}
const settle = () => new Promise(resolve => setImmediate(resolve));
for (const board of ['alpha250','alpha250-4','red-pitaya']) {
    test(`${board} legacy save reports requests and resets feedback after the latest click`, async t => {
        const h = fixture(t, board);
        assert.equal(h.calls, 0);
        h.button.click();
        await settle();
        assert.equal(h.calls, 1); assert.equal(h.button.textContent, 'Save requested');
        assert.equal(h.status.textContent, 'Analyzer settings save requested.');
        const oldTimer = h.timers.keys().next().value;
        h.button.click(); await settle(); assert.equal(h.calls, 2);
        assert(!h.timers.has(oldTimer));
        h.tick(); assert.equal(h.button.textContent, 'Save settings');
        assert.equal(h.status.textContent, 'Analyzer settings save requested.');
    });
    test(`${board} save failure reports the error and disposal removes commands and timers`, async t => {
        const h = fixture(t, board); h.fail = true; h.button.click();
        await settle();
        assert.equal(h.calls, 1); assert.equal(h.errors.length, 1);
        assert.equal(h.button.textContent, 'Save failed');
        assert.equal(h.status.textContent, 'Unable to confirm the save request.');
        h.tick(); h.fail = false; h.button.click();
        await settle();
        assert.equal(h.button.textContent, 'Save requested');
        const stale = h.timers.values().next().value.callback;
        h.action.dispose(); h.action.dispose();
        assert.equal(h.timers.size, 0); h.button.click(); stale();
        assert.equal(h.calls, 2); assert.equal(h.button.textContent, 'Save requested');
    });
    test(`${board} checked save waits for confirmation and prevents duplicate requests`, async t => {
        const h = fixture(t, board);
        let confirm;
        h.result = new Promise(resolve => {confirm = resolve;});
        h.button.click();
        assert.equal(h.button.textContent, 'Saving…');
        assert.equal(h.button.disabled, true);
        assert.equal(h.timers.size, 0);
        h.button.dispatchEvent(new h.button.ownerDocument.defaultView.Event('click'));
        assert.equal(h.calls, 1);
        confirm(true); await settle();
        assert.equal(h.button.disabled, false);
        assert.equal(h.button.textContent, 'Saved');
        assert.equal(h.status.textContent, 'Analyzer settings saved.');
        h.tick(); assert.equal(h.button.textContent, 'Save settings');
    });
    test(`${board} persistence failure is reported without a connection fault`, async t => {
        const h = fixture(t, board); h.result = Promise.resolve(false);
        h.button.click(); await settle();
        assert.equal(h.button.textContent, 'Save failed');
        assert.match(h.status.textContent, /could not be saved/);
        assert.equal(h.errors.length, 0);
        assert.equal(h.button.disabled, false);
        h.tick();
        h.result = Promise.resolve(true);
        h.button.click(); await settle();
        assert.equal(h.button.textContent, 'Saved');
        assert.equal(h.calls, 2);
    });
    test(`${board} late save replies and failures do not update disposed controls`, async t => {
        for (const fail of [false, true]) {
            const h = fixture(t, board);
            let resolve, reject;
            h.result = new Promise((yes, no) => {resolve = yes; reject = no;});
            h.button.click(); h.action.dispose();
            const text = h.status.textContent;
            if (fail) reject(new Error('Connection closed')); else resolve(true);
            await settle();
            assert.equal(h.button.textContent, 'Saving…');
            assert.equal(h.status.textContent, text);
            assert.equal(h.errors.length, 0);
            assert.equal(h.timers.size, 0);
            assert.equal(h.button.disabled, false);
        }
    });
}
test('connection-error teardown during a save does not schedule another timer', async t => {
    const h = fixture(t, 'alpha250-4'); h.onError = () => h.action.dispose();
    h.fail = true; h.button.click();
    await settle();
    assert.equal(h.errors.length, 1); assert.equal(h.timers.size, 0);
    h.button.click(); assert.equal(h.calls, 1);
});
test('an asynchronously rejected save reports the connection failure', async t => {
    const h = fixture(t, 'alpha250');
    h.result = Promise.reject(new Error('Connection closed'));
    h.button.click(); await settle();
    assert.equal(h.errors.length, 1);
    assert.equal(h.button.textContent, 'Save failed');
    assert.equal(h.button.disabled, false);
    h.tick();
});

for (const source of ['web/phase-noise/analyzer/phase-noise-analyzer.ts',
                     'examples/alpha250-4/phase-noise-analyzer/web/phase-noise-analyzer.ts']) {
    function driverFixture(t, checked) {
        const dom = new JSDOM('', {runScripts:'outside-only'});
        t.after(() => dom.window.close());
        const w = dom.window, calls = [], commands = {save_config: {name:'legacy'}};
        if (checked) commands.save_config_checked = {name:'checked'};
        let reply = true;
        w.PnaSpectrumReader = class {};
        w.Command = (id, command) => ({id, command});
        w.eval(ts.transpileModule(fs.readFileSync(path.join(root, source), 'utf8'),
            {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText + '\nwindow.PhaseNoiseAnalyzer = PhaseNoiseAnalyzer;');
        const client = {
            getDriver: () => ({id:42, getCmds:() => commands}),
            readBool: async request => {calls.push(['read', request.command.name]); return reply;},
            send: request => calls.push(['send', request.command.name])
        };
        return {driver:new w.PhaseNoiseAnalyzer(client), calls, set reply(value){reply=value;}};
    }
    test(`${source} uses the checked save reply when available`, async t => {
        const h = driverFixture(t, true);
        assert.equal(await h.driver.saveConfig(), true);
        h.reply = false;
        assert.equal(await h.driver.saveConfig(), false);
        assert.deepEqual(h.calls, [['read','checked'], ['read','checked']]);
    });
    test(`${source} retains request-only fallback for older servers`, async t => {
        const h = driverFixture(t, false);
        assert.equal(await h.driver.saveConfig(), undefined);
        assert.deepEqual(h.calls, [['send','legacy']]);
    });
}
