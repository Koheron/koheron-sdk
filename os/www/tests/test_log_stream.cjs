const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {JSDOM} = require('jsdom');
const ts = require('../../../web/transpile.cjs');
const code = ts.transpileModule(fs.readFileSync('os/www/koheron_server_log.ts', 'utf8')).outputText;
function fixture(t, url = 'http://board:80/koheron/') {
    const dom = new JSDOM(fs.readFileSync('os/www/index.html', 'utf8'), {url, runScripts: 'outside-only', pretendToBeVisual: true});
    t.after(() => dom.window.close());
    const w = dom.window, sockets = [], reads = [], timers = new Map(); let id = 0;
    w.setTimeout = (fn, delay) => { timers.set(++id, {fn, delay}); return id; };
    w.clearTimeout = id => timers.delete(id);
    w.WebSocket = class {
        constructor(url) { this.url = url; sockets.push(this); }
        close() { this.closed = true; }
        reply(value) { this.onmessage({data: JSON.stringify(value)}); }
    };
    w.fetch = (url, options) => new Promise(resolve => reads.push({
        url, options, reply: value => resolve({ok: true, text: async () => JSON.stringify(value)})
    }));
    w.eval(code + '\nObject.assign(window, {KoheronLog, KoheronLogWidget}); new KoheronLogWidget(document);');
    const fire = delay => {
        const item = [...timers.entries()].find(([, timer]) => timer.delay === delay);
        assert.ok(item, 'Missing timer ' + delay); timers.delete(item[0]); item[1].fn();
    };
    return {w, doc: w.document, sockets, reads, timers, fire, flush: () => new Promise(resolve => setImmediate(resolve))};
}
const batch = (cursor, messages = [], reset = false) => ({
    type: 'logs', cursor, reset, entries: messages.map(msg => ({msg, ts: null, prio: 6}))
});

test('live WebSocket batches and heartbeats never poll HTTP or duplicate messages', t => {
    const f = fixture(t);
    assert.equal(f.sockets[0].url, 'ws://board/api/logs/koheron/events');
    f.sockets[0].reply(batch('a', ['ready']));
    f.sockets[0].reply(batch('a', ['ready']));
    f.sockets[0].reply(batch('a'));
    assert.equal(f.reads.length, 0);
    assert.equal(f.doc.querySelector('#koheron-log').textContent.split('\n').length, 1);
    assert.equal(f.doc.querySelector('#log-status').textContent, 'Live');
    f.sockets[0].reply(batch('b', ['second']));
    assert.equal(f.doc.querySelector('#koheron-log').textContent.split('\n').length, 2);
});

test('pause releases the stream, ignores late frames and resumes at its cursor', t => {
    const f = fixture(t), socket = f.sockets[0];
    socket.reply(batch('s=a;b=c', ['before'])); const late = socket.onmessage;
    f.doc.querySelector('#log-pause').click();
    assert.equal(socket.closed, true);
    late({data: JSON.stringify(batch('b', ['late']))});
    assert.equal(f.doc.querySelector('#log-status').textContent, 'Paused');
    f.doc.querySelector('#log-pause').click();
    assert.match(f.sockets[1].url, /cursor=s%3Da%3Bb%3Dc$/);
    f.sockets[1].reply(batch('b', ['after']));
    assert.match(f.doc.querySelector('#koheron-log').textContent, /before[\s\S]*after/);
    assert.doesNotMatch(f.doc.querySelector('#koheron-log').textContent, /late/);
});

test('failed socket uses bounded HTTP and recovery cancels stale fallback reads', async t => {
    const f = fixture(t);
    f.sockets[0].reply(batch('a', ['first'])); f.sockets[0].onclose();
    assert.match(f.reads[0].url, /tail\?cursor=a$/);
    f.reads.shift().reply(batch('b', ['fallback'])); await f.flush();
    assert.equal(f.doc.querySelector('#log-status').textContent, 'Live · polling');
    f.fire(1000); // reconnect before the next poll
    f.fire(1000); // outstanding fallback read
    const old = f.reads.shift();
    f.sockets[1].reply(batch('c', ['websocket']));
    assert.equal(old.options.signal.aborted, true);
    old.reply(batch('d', ['obsolete'])); await f.flush();
    assert.doesNotMatch(f.doc.querySelector('#koheron-log').textContent, /obsolete/);
    assert.equal(f.doc.querySelector('#log-status').textContent, 'Live');
});

test('fallback cursor advancing during handshake reopens without replaying old history', async t => {
    const f = fixture(t);
    f.sockets[0].reply(batch('a', ['first'])); f.sockets[0].onclose();
    f.fire(1000); // WS requests cursor a
    f.reads.shift().reply(batch('b', ['once'])); await f.flush();
    f.sockets[1].reply(batch('b', ['once']));
    assert.equal(f.sockets[1].closed, true);
    assert.match(f.sockets[2].url, /cursor=b$/);
    f.sockets[2].reply(batch('b'));
    assert.doesNotMatch(f.doc.querySelector('#koheron-log').textContent, /×2/);
});

test('blocked constructor retries while HTTP reads remain available', async t => {
    const f = fixture(t);
    f.doc.querySelector('#log-pause').click();
    f.w.WebSocket = class { constructor() { throw new Error('Blocked'); } };
    f.doc.querySelector('#log-pause').click();
    f.reads.shift().reply(batch('a', ['fallback'])); await f.flush();
    assert.equal(f.doc.querySelector('#log-status').textContent, 'Live · polling');
    f.fire(1000);
    assert.equal(f.reads.length, 1);
});

test('stale journal cursor clears old history and safely renders priority and text', t => {
    const f = fixture(t);
    f.sockets[0].reply(batch('a', ['old']));
    const value = batch('b', ['<img src=x>'], true); value.entries[0].prio = 3;
    f.sockets[0].reply(value);
    assert.doesNotMatch(f.doc.querySelector('#koheron-log').textContent, /old/);
    assert.equal(f.doc.querySelector('#koheron-log img'), null);
    assert.equal(f.doc.querySelector('.log-line').dataset.level, 'error');
});

test('malformed or oversized batches reconnect without rendering', t => {
    const f = fixture(t);
    f.sockets[0].reply(batch('a', ['valid']));
    f.sockets[0].reply({...batch('b'), entries: [{msg: '<bad>', ts: -1}]});
    assert.equal(f.sockets[0].closed, true);
    assert.doesNotMatch(f.doc.querySelector('#koheron-log').textContent, /bad/);
    f.fire(1000);
    f.sockets[1].onmessage({data: 'x'.repeat(65537)});
    assert.equal(f.sockets[1].closed, true);
});

test('hidden page and BFCache suspend the stream and resume from the last cursor', t => {
    const f = fixture(t);
    f.sockets[0].reply(batch('a', ['first']));
    Object.defineProperty(f.doc, 'hidden', {configurable: true, value: true});
    f.doc.dispatchEvent(new f.w.Event('visibilitychange'));
    assert.equal(f.sockets[0].closed, true);
    Object.defineProperty(f.doc, 'hidden', {configurable: true, value: false});
    f.doc.dispatchEvent(new f.w.Event('visibilitychange'));
    assert.match(f.sockets[1].url, /cursor=a$/);
    f.w.dispatchEvent(new f.w.PageTransitionEvent('pagehide', {persisted: true}));
    assert.equal(f.sockets[1].closed, true);
    f.w.dispatchEvent(new f.w.PageTransitionEvent('pageshow', {persisted: true}));
    assert.match(f.sockets[2].url, /cursor=a$/);
});

test('watchdog reconnects stalled streams and timed-out HTTP releases its read lock', async t => {
    const f = fixture(t);
    f.fire(5000);
    const read = f.reads.shift();
    f.fire(8000); assert.equal(read.options.signal.aborted, true);
    read.reply(batch('a', ['timed out'])); await f.flush();
    assert.equal(f.doc.querySelector('#log-status').textContent, 'Disconnected · retrying…');
    assert.doesNotMatch(f.doc.querySelector('#koheron-log').textContent, /timed out/);
    // A real fetch rejects on abort; even a late resolve cannot leave the lock held.
    assert.ok([...f.timers.values()].some(timer => timer.delay === 1000));
});

test('HTTPS pages use a secure WebSocket on their original host and port', t => {
    const f = fixture(t, 'https://board:8443/koheron/');
    assert.equal(f.sockets[0].url, 'wss://board:8443/api/logs/koheron/events');
});
