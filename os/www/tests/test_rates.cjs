const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {JSDOM} = require('jsdom');
const ts = require('typescript');
const code = ts.transpileModule(fs.readFileSync('os/www/logs_rate.ts', 'utf8'),
    {compilerOptions: {target: ts.ScriptTarget.ES2018}}).outputText;
function fixture(t) {
    const dom = new JSDOM(fs.readFileSync('os/www/logs_rate.html', 'utf8'), {
        url: 'http://board/koheron/logs_rate.html', runScripts: 'outside-only', pretendToBeVisual: true
    });
    t.after(() => dom.window.close());
    const w = dom.window, doc = w.document, calls = [];
    const canvas = doc.querySelector('canvas');
    canvas.getBoundingClientRect = () => ({width: 600, height: 260});
    const context = {};
    for (const name of ['setTransform','clearRect','beginPath','moveTo','lineTo','stroke','fillText','save','rect','clip','arc','fill','restore']) {
        context[name] = (...args) => calls.push({name, args});
    }
    canvas.getContext = () => context;
    w.eval(code + '\nObject.assign(window, {LogsRateClient, LogsRateChart, LogsRateTable, LogsRatePage, formatRateBytes});');
    return {w, doc, canvas, calls, flush: () => new Promise(resolve => setImmediate(resolve))};
}
const session = {id:1, name:'WebSocket', rx_inst:8192, tx_inst:16384,
    rx_mean:4096, tx_mean:8192, rx_max:16384, tx_max:32768, rx_total:1024, tx_total:2048};

test('table converts bits/s once, leaves totals in bytes and safely renders names', t => {
    const {w, doc} = fixture(t);
    const table = new w.LogsRateTable(doc.querySelector('table'));
    table.render([{...session,name:'<img src=x>'}]);
    const cells = [...doc.querySelectorAll('tbody td')].map(cell => cell.textContent);
    assert.deepEqual(cells, ['1','<img src=x>','1.00 KiB/s','512 B/s','2.00 KiB/s','1.00 KiB',
        '2.00 KiB/s','1.00 KiB/s','4.00 KiB/s','2.00 KiB']);
    assert.equal(doc.querySelector('table img'), null);
    table.render([]); assert.equal(doc.querySelector('tbody td').colSpan, 10);
    assert.equal(doc.querySelector('tbody td').textContent, 'No connected sessions');
    assert.equal(w.formatRateBytes(-1, true), '0 B/s');
    assert.equal(w.formatRateBytes(NaN), '0 B');
});

test('chart uses canvas CSS dimensions at device scale and keeps axes inside bounds', t => {
    const {w, canvas, calls} = fixture(t);
    Object.defineProperty(w, 'devicePixelRatio', {value: 2});
    const chart = new w.LogsRateChart(canvas); chart.resize(); chart.addSample(1024,2048,100);
    assert.equal(canvas.width, 1200); assert.equal(canvas.height, 520);
    assert.deepEqual(calls.find(call => call.name === 'setTransform').args, [2,0,0,2,0,0]);
    const labels = calls.filter(call => call.name === 'fillText');
    assert(labels.some(call => call.args[0] === '−4 min'));
    assert(labels.some(call => call.args[0] === 'Latest'));
    for (const {args} of labels) { assert(args[1] >= 0 && args[1] <= 600); assert(args[2] >= 0 && args[2] < 260); }
    assert.equal(calls.filter(call => call.name === 'arc').length, 2);
});

test('chart rejects repeated timestamps, breaks gaps, bounds history and resets after reboot', t => {
    const {w, canvas, calls} = fixture(t);
    const chart = new w.LogsRateChart(canvas, 3); chart.resize();
    chart.addSample(10,20,100); const count = calls.length;
    chart.addSample(10,20,100); assert.equal(calls.length, count);
    chart.addSample(10,20,102);
    calls.length = 0; chart.addSample(10,20,120);
    // Two grid/series paths per stream; a new subpath begins after the missing interval.
    const seriesStart = calls.findIndex(call => call.name === 'clip');
    const series = calls.slice(seriesStart);
    assert.equal(series.filter(call => call.name === 'moveTo').length, 4);
    assert.equal(series.filter(call => call.name === 'lineTo').length, 2);
    chart.addSample(10,20,122); assert.equal(chart.samples.length, 3);
    chart.addSample(10,20,400); assert.equal(chart.samples.length, 1);
    chart.addSample(10,20,1); assert.equal(chart.samples.length, 1);
});

test('poller prevents overlap and ignores in-flight replies after stop, then resumes', async t => {
    const {w, flush} = fixture(t);
    let complete, reads = 0, updates = 0;
    const timers = new Map(); let timerId = 0;
    w.setTimeout = (callback, delay) => { timers.set(++timerId, {callback, delay}); return timerId; };
    w.clearTimeout = id => timers.delete(id);
    w.fetch = () => { reads++; return new Promise(resolve => complete = resolve); };
    const client = new w.LogsRateClient('/rates',2000,() => updates++);
    client.start(); client.start(); assert.equal(reads,1);
    client.stop(); complete({ok:true,json:async() => ({ts:1,sessions:[]})}); await flush();
    assert.equal(updates,0); assert.equal(timers.size,0);
    client.start(); complete({ok:true,json:async() => ({ts:2,sessions:[]})}); await flush();
    assert.equal(updates,1); assert.equal([...timers.values()][0].delay,2000);
    client.stop(); assert.equal(timers.size,0);
});

test('poller retries invalid data and suspends reads when the page is hidden', async t => {
    const {w, doc, flush} = fixture(t);
    let retry, errors = 0, updates = 0, reads = 0;
    w.setTimeout = (callback, delay) => { if (delay === 2000) retry = callback; return 1; };
    w.clearTimeout = () => {};
    w.fetch = async () => { reads++; return {ok:true,json:async() => ({ts:'bad',sessions:[]})}; };
    const client = new w.LogsRateClient('/rates',2000,() => updates++,() => errors++);
    client.start(); await flush(); assert.equal(errors,1); assert.equal(updates,0);
    Object.defineProperty(doc,'hidden',{configurable:true,value:true}); retry(); await flush();
    assert.equal(reads,1);
    Object.defineProperty(doc,'hidden',{configurable:true,value:false});
    w.fetch = async () => ({ok:true,json:async() => ({ts:2,sessions:[]})});
    retry(); await flush(); assert.equal(updates,1); client.stop();
});

test('page counts current transfers, detects stale snapshots, pauses and recovers', async t => {
    const {w, doc, flush} = fixture(t);
    let now = 0; w.performance.now = () => now;
    w.fetch = async () => ({ok:true,json:async() => ({ts:10,sessions:[session,
        {...session,id:2,rx_inst:0,tx_inst:0}]})});
    const page = new w.LogsRatePage(doc); await flush();
    assert.equal(doc.querySelector('#rate-sessions').textContent, '2');
    assert.equal(doc.querySelector('#rate-active').textContent, '1');
    assert.equal(doc.querySelector('#rate-rx').textContent, '1.00 KiB/s');
    assert.equal(doc.querySelector('#rate-rx-total').textContent, '2.00 KiB');
    now=7000; page.handleUpdate({ts:10,sessions:[session]});
    assert.equal(doc.querySelector('#logs-rate-status').dataset.state,'error');
    assert.match(doc.querySelector('#logs-rate-status').textContent,/No new data/);
    assert.equal(page.chart.samples.length,1);
    page.handleUpdate({ts:11,sessions:[]});
    assert.equal(doc.querySelector('#logs-rate-status').dataset.state,'live');
    assert.equal(doc.querySelector('#rate-active').textContent,'0');
    doc.querySelector('#logs-rate-pause').click();
    assert.equal(doc.querySelector('#logs-rate-status').textContent,'Paused');
    assert.equal(doc.querySelector('#logs-rate-pause').getAttribute('aria-pressed'),'true');
    doc.querySelector('#logs-rate-pause').click(); await flush();
    assert.equal(doc.querySelector('#logs-rate-status').dataset.state,'live');
    page.client.stop();
});
