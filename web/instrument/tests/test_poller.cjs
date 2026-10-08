const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('typescript');
const root = path.resolve(__dirname, '../../..');
const settle = async () => { for (let i = 0; i < 8; i++) { await Promise.resolve(); } };

function fixture(read) {
    let id = 0;
    const timers = new Map(), rendered = [], errors = [];
    const document = {hidden: false, defaultView: {
        setTimeout(callback, delay) { timers.set(++id, {callback, delay}); return id; },
        clearTimeout(key) { timers.delete(key); }
    }};
    const context = vm.createContext({console});
    vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(root, 'web/instrument/poller.ts'), 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText + '\nglobalThis.InstrumentPoller = InstrumentPoller;', context);
    const poller = new context.InstrumentPoller(document, read, value => rendered.push(value), error => errors.push(error));
    const tick = async () => {
        assert.equal(timers.size, 1);
        const timer = timers.values().next().value;
        assert.equal(timer.delay, 1000);
        await timer.callback();
    };
    return {poller, document, timers, rendered, errors, tick};
}

test('slow telemetry uses one timer, pauses reads while hidden and retries failures', async () => {
    let reads = 0, fail = false;
    const h = fixture(async () => { reads++; if (fail) { throw new Error('Unavailable'); } return reads; });
    assert.equal(reads, 0);
    h.poller.start(); h.poller.start();
    await settle();
    assert.equal(reads, 1);
    assert.deepEqual(h.rendered, [1]);
    h.document.hidden = true;
    await h.tick();
    assert.equal(reads, 1);
    h.document.hidden = false; fail = true;
    await h.tick();
    assert.equal(h.errors.length, 1);
    assert.deepEqual(h.rendered, [1]);
    fail = false;
    await h.tick();
    assert.deepEqual(h.rendered, [1, 3]);
    h.poller.dispose(); h.poller.dispose(); h.poller.start();
    assert.equal(h.timers.size, 0);
});

test('pending telemetry never overlaps and disposal discards late results and errors', async () => {
    for (const failure of [false, true]) {
        let resolve, reject, reads = 0;
        const h = fixture(() => { reads++; return new Promise((yes, no) => { resolve = yes; reject = no; }); });
        h.poller.start(); h.poller.start();
        await h.poller.poll();
        assert.equal(reads, 1);
        assert.equal(h.timers.size, 0);
        h.poller.dispose();
        if (failure) { reject(new Error('Disconnected')); } else { resolve(42); }
        await settle();
        assert.deepEqual(h.rendered, []);
        assert.deepEqual(h.errors, []);
        assert.equal(h.timers.size, 0);
    }
});
