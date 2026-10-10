const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {JSDOM} = require('jsdom');
const ts = require('../../../web/transpile.cjs');
const sources = ['web/instrument/poller.ts', 'os/www/instruments.ts',
    'os/www/runtime.ts',
    'os/www/instruments_widget.ts', 'os/www/koheron_server_log.ts',
    'os/www/koheron_system.ts', 'os/www/system_info_widget.ts', 'os/www/instrument_summary.ts'];
const code = ts.transpileModule(sources.map(path => fs.readFileSync(path, 'utf8')).join('\n'),
    {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText;
function fixture(t, page = 'index.html', query = '') {
    const dom = new JSDOM(fs.readFileSync('os/www/' + page, 'utf8'), {
        url: 'http://board/koheron/' + page + query, runScripts: 'outside-only', pretendToBeVisual: true
    });
    t.after(() => dom.window.close());
    const w = dom.window;
    const requests = [];
    w.XMLHttpRequest = class {
        open(method, url) { this.method = method; this.url = url; }
        send(body) { this.body = body; requests.push(this); }
        reply(status, value) {
            this.status = status; this.readyState = 4;
            this.responseText = typeof value === 'string' ? value : JSON.stringify(value);
            this.onload();
        }
    };
    w.eval(code + "\nObject.assign(window, { Instruments, InstrumentsWidget, KoheronLogWidget, SystemInfoWidget, InstrumentSummaryWidget });");
    return {w, requests, doc: w.document, flush: () => new Promise(resolve => setImmediate(resolve))};
}
const installed = {instruments: [
    {name: 'fft', version: '1.0', is_default: true},
    {name: 'scope', version: '<img src=x onerror=alert(1)>', is_default: false}
], live_instrument: null};

test('idle board, safe metadata, protected default and stable focus across polling', async t => {
    const {w, requests, doc, flush} = fixture(t);
    w.eval('new InstrumentsWidget(document)');
    requests.shift().reply(200, installed); await flush();
    assert.equal(doc.querySelector('#live-name').textContent, 'No instrument running');
    assert.equal(doc.querySelector('#open-instrument').hidden, true);
    assert.equal(doc.querySelectorAll('tbody img').length, 0);
    assert.equal(doc.querySelectorAll('.remove').length, 1);
    const button = doc.querySelector('tbody button'); button.focus();
    doc.querySelector('#refresh-instruments').click();
    requests.shift().reply(200, installed); await flush();
    assert.equal(doc.activeElement, button);
});

test('failed run stays on manager, shows error and restores controls', async t => {
    const {w, requests, doc, flush} = fixture(t);
    w.eval('new InstrumentsWidget(document)');
    requests.shift().reply(200, installed); await flush();
    doc.querySelector('tbody button').click();
    assert.equal(doc.querySelector('#upload-btn').disabled, true);
    requests.shift().reply(500, 'failed');
    assert.match(doc.querySelector('#upload-status').textContent, /HTTP 500/);
    assert.equal(doc.querySelector('#upload-btn').disabled, false);
    assert.match(w.location.pathname, /index.html/);
    requests.shift().reply(200, installed); await flush();
    doc.querySelector('tbody button').click();
    requests.shift().reply(200, 'started');
    requests.shift().reply(200, {...installed, live_instrument: installed.instruments[0]}); await flush();
    assert.equal(doc.querySelector('#live-name').textContent, 'fft');
    assert.equal(doc.querySelector('#open-instrument').hidden, false);
});

test('remove waits for server completion and honors cancellation', async t => {
    const {w, requests, doc, flush} = fixture(t);
    w.eval('new InstrumentsWidget(document)');
    requests.shift().reply(200, installed); await flush();
    w.confirm = () => false; doc.querySelector('.remove').click(); assert.equal(requests.length, 0);
    w.confirm = () => true; doc.querySelector('.remove').click();
    assert.equal(requests.length, 1); assert.match(requests[0].url, /delete\/scope$/);
    requests.shift().reply(200, 'removed');
    assert.equal(requests.length, 1); assert.match(requests[0].url, /details$/);
    requests.shift().reply(200, {instruments: [], live_instrument: null}); await flush();
    assert.match(doc.querySelector('tbody').textContent, /No instruments installed/);
});

test('upload handles cancelled picker, invalid extension, error and retry', async t => {
    const {w, requests, doc, flush} = fixture(t);
    w.eval('new InstrumentsWidget(document)'); requests.shift().reply(200, installed); await flush();
    const input = doc.querySelector('#upload-input');
    const select = name => {
        Object.defineProperty(input, 'files', {configurable: true, value: name ? [new w.File(['zip'], name)] : []});
        input.dispatchEvent(new w.Event('change'));
    };
    select(null); assert.equal(requests.length, 0);
    select('bad.zip.txt'); assert.equal(requests.length, 0);
    assert.match(doc.querySelector('#upload-status').textContent, /Select an instrument ZIP/);
    select('fft.ZIP'); assert.match(requests[0].url, /upload$/);
    requests.shift().onerror(); assert.match(doc.querySelector('#upload-status').textContent, /Cannot reach/);
    requests.shift().reply(200, installed); await flush();
    select('fft.ZIP'); requests.shift().reply(200, 'ok');
    assert.match(doc.querySelector('#upload-status').textContent, /uploaded/);
});

test('status errors recover and encoded instrument names stay intact', async t => {
    const {w, requests, doc, flush} = fixture(t);
    w.eval('new InstrumentsWidget(document)'); requests.shift().reply(200, 'broken json'); await flush();
    assert.equal(doc.querySelector('#instruments-status').dataset.state, 'error');
    doc.querySelector('#refresh-instruments').click();
    requests.shift().reply(200, installed); await flush();
    assert.equal(doc.querySelector('#instruments-status').dataset.state, 'ready');
    w.eval('new Instruments().runInstrument("a/b %", () => {})');
    assert.match(requests.shift().url, /a%2Fb%20%25$/);
});

test('log pause survives in-flight fetch; resume and bounded grouping work', async t => {
    const {w, doc, flush} = fixture(t);
    let complete;
    w.WebSocket = class { constructor() { throw new Error('Blocked'); } };
    w.fetch = () => new Promise(resolve => { complete = resolve; });
    w.eval('new KoheronLogWidget(document)');
    doc.querySelector('#log-pause').click();
    complete({ok: true, text: async () => JSON.stringify({type: 'logs', reset: false, cursor: '1', entries: [{ts: null, msg: 'late'}]})});
    await flush();
    assert.equal(doc.querySelector('#koheron-log').textContent, '');
    assert.equal(doc.querySelector('#log-status').textContent, 'Paused');
    doc.querySelector('#log-pause').click();
    complete({ok: true, text: async () => JSON.stringify({type: 'logs', reset: false, cursor: '2', entries: [{ts: null, msg: 'ready'}]})});
    await flush(); assert.match(doc.querySelector('#koheron-log').textContent, /ready/);
    w.eval(`const format = KoheronLogWidget.prototype.makeCoalescingFormatter(document.querySelector('#koheron-log'), () => false);
        format(Array.from({length: 1200}, (_, i) => ({ts: null, msg: String(i)})));
        format([{ts: null, msg: '1199'}]);`);
    const lines = doc.querySelector('#koheron-log').textContent.split('\n');
    assert.equal(lines.length, 1000); assert.match(lines[999], /×2/);
});

test('system metadata is text and has a working retry', async t => {
    const {w, doc, flush} = fixture(t);
    w.fetch = async () => { throw new Error('offline'); };
    w.eval('new SystemInfoWidget(document)'); await flush();
    assert.equal(doc.querySelector('#system-info-retry').hidden, false);
    w.fetch = async () => ({ok: true, json: async () => ({release: {NAME: '<img src=x>'}, manifest: {}})});
    doc.querySelector('#system-info-retry').click(); await flush();
    assert.match(doc.querySelector('#release-table').textContent, /<img src=x>/);
    assert.equal(doc.querySelector('#release-table img'), null);
});

test('summary accepts literal percent in instrument name', async t => {
    const {w, requests, doc, flush} = fixture(t, 'instrument_summary.html', '?name=fft%25test');
    w.eval('new InstrumentSummaryWidget(document)');
    assert.equal(doc.querySelector('#instrument-name').textContent, 'fft%test');
    assert.match(requests[1].url, /fft%25test$/);
    requests[2].reply(200, {ready: true, warnings: [], archive: {}});
    await flush();
});

test('summary without a name offers a clear route back without perpetual loading', t => {
    const {w, requests, doc} = fixture(t, 'instrument_summary.html');
    w.eval('new InstrumentSummaryWidget(document)');
    assert.equal(requests.length, 0);
    assert.equal(doc.querySelector('#instrument-name').textContent, 'No instrument selected');
    assert.match(doc.querySelector('#instrument-details-status').textContent, /Choose an instrument/);
    assert.equal(doc.querySelector('#instrument-check-heading').closest('section').hidden, true);
    assert.equal(doc.querySelector('#commands-heading').closest('section').hidden, true);
});

test('summary check prevents overlapping requests and re-enables retry after failure', async t => {
    const {w, requests, doc, flush} = fixture(t, 'instrument_summary.html', '?name=fft');
    w.eval('new InstrumentSummaryWidget(document)');
    const button = doc.querySelector('#instrument-check-refresh');
    assert.equal(button.disabled, true); button.click(); assert.equal(requests.length, 3);
    requests[2].reply(500, {error: 'Cannot check the instrument'}); await flush();
    assert.equal(button.disabled, false); assert.equal(button.textContent, 'Check again');
    button.click(); assert.equal(requests.length, 4);
    requests[3].reply(200, {ready: true}); await flush();
    assert.match(doc.querySelector('#instrument-check').textContent, /Ready to run/);
});

test('commands timeout has a working retry and cannot erase an independent details error', t => {
    const {w, requests, doc} = fixture(t, 'instrument_summary.html', '?name=fft');
    w.eval('new InstrumentSummaryWidget(document)');
    requests[0].reply(200, {instruments: []});
    assert.equal(requests[1].timeout, 8000); requests[1].ontimeout();
    const retry = doc.querySelector('#instrument-commands-retry'); assert.equal(retry.hidden, false);
    retry.click(); assert.equal(requests.length, 4); assert.equal(retry.hidden, true);
    requests[3].reply(200, [{class: 'Common', functions: [{name: '<img src=x>', args: [], ret_type: 'void'}]}]);
    assert.equal(doc.querySelector('#instrument-commands-status').hidden, true);
    assert.match(doc.querySelector('#instrument-details-status').textContent, /no longer installed/);
    assert.match(doc.querySelector('#instrument-commands').textContent, /<img src=x>/);
    assert.equal(doc.querySelector('#instrument-commands img'), null);
});

test('command description distinguishes unavailable metadata from malformed metadata', t => {
    const {w, requests, doc} = fixture(t, 'instrument_summary.html', '?name=fft');
    w.eval('new InstrumentSummaryWidget(document)');
    requests[1].reply(200, {});
    assert.equal(doc.querySelector('#instrument-commands-retry').hidden, false);
    doc.querySelector('#instrument-commands-retry').click();
    requests[3].reply(404, 'Missing');
    assert.equal(doc.querySelector('#instrument-commands-retry').hidden, true);
    assert.match(doc.querySelector('#instrument-commands-status').textContent, /No command description/);
});

