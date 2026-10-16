const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {JSDOM} = require('jsdom');
const ts = require('../../../web/transpile.cjs');
const sources = ['web/instrument/poller.ts', 'os/www/instruments.ts', 'os/www/runtime.ts',
    'os/www/instruments_widget.ts', 'os/www/koheron_system.ts', 'os/www/system_info_widget.ts'];
const code = ts.transpileModule(sources.map(path => fs.readFileSync(path, 'utf8')).join('\n')).outputText;
const installed = [{name: 'fft', version: '1', is_default: true}, {name: 'scope', version: '2', is_default: false}];
function status(overrides = {}) {
    return {type: 'status', instruments: {instruments: installed, live_instrument: installed[0]},
        current_instrument: installed[0], operation: {phase: 'idle', busy: false, revision: 0},
        health: {uptime_seconds: 100, load_average: [0, .1, .2], memory: {available_bytes: 100000000},
            storage: {instruments: {available_bytes: 200000000}, staging: {available_bytes: 100000000}},
            instrument_service: {state: 'active'}, timing: {api_ready_monotonic_us: 1000000}}, ...overrides};
}
function fixture(t) {
    const dom = new JSDOM(fs.readFileSync('os/www/index.html', 'utf8'), {url: 'http://board:8080/koheron/', runScripts: 'outside-only', pretendToBeVisual: true});
    t.after(() => dom.window.close());
    const w = dom.window, requests = [], sockets = [], timers = new Map(); let id = 0;
    w.setTimeout = (fn, delay) => { timers.set(++id, {fn, delay}); return id; };
    w.clearTimeout = id => timers.delete(id);
    w.fetch = async () => ({ok: true, json: async () => ({manifest: {}, release: {}})});
    w.WebSocket = class {
        constructor(url) { this.url = url; sockets.push(this); }
        close() { this.closed = true; }
        reply(value) { this.onmessage({data: JSON.stringify(value)}); }
    };
    w.XMLHttpRequest = class {
        open(method, url) { this.method = method; this.url = url; }
        send() { requests.push(this); }
        abort() { this.aborted = true; if (this.onabort) this.onabort(); }
        reply(status, value) { this.status = status; this.responseText = JSON.stringify(value); this.onload(); }
    };
    w.eval(code + '\nObject.assign(window, {RuntimeStream, InstrumentsWidget, SystemInfoWidget, PreflightView});');
    w.eval('window.runtime = new RuntimeStream(document); new InstrumentsWidget(document, runtime); new SystemInfoWidget(document, runtime);');
    return {w, doc: w.document, requests, sockets, timers, flush: () => new Promise(resolve => setImmediate(resolve))};
}

test('one WebSocket drives coherent status, health and lifecycle controls', async t => {
    const f = fixture(t); assert.equal(f.sockets.length, 1); assert.equal(f.sockets[0].url, 'ws://board:8080/api/events');
    f.sockets[0].reply(status());
    assert.equal(f.doc.querySelector('#live-name').textContent, 'fft');
    assert.equal(f.doc.querySelector('#instrument-stop').hidden, false);
    assert.equal(f.doc.querySelector('#instrument-start').hidden, true);
    assert.match(f.doc.querySelector('#health-table').textContent, /95.4 MiB/);
    const button = f.doc.querySelector('#instrument-stop'); button.focus();
    f.sockets[0].reply(status()); assert.equal(f.doc.activeElement, button);
    button.click(); assert.equal(f.requests[0].method, 'POST'); assert.match(f.requests[0].url, /control\/stop$/);
    f.requests.shift().reply(200, {message: 'Instrument stopped'}); await f.flush();
    f.sockets[0].reply(status({instruments: {instruments: installed, live_instrument: null}}));
    assert.equal(f.doc.querySelector('#live-label').textContent, 'Stopped');
    assert.equal(f.doc.querySelector('#instrument-start').hidden, false);
});

test('activation progress disables mutations and reports rollback', t => {
    const f = fixture(t); f.sockets[0].reply(status());
    f.sockets[0].reply(status({operation: {phase: 'rolling_back', instrument: 'scope', busy: true, revision: 1}}));
    assert.match(f.doc.querySelector('#runtime-operation').textContent, /Restoring/);
    assert.equal(f.doc.querySelector('#upload-btn').disabled, true);
    assert.equal(f.doc.querySelector('#open-instrument').hidden, true);
    f.sockets[0].reply(status({operation: {phase: 'failed', busy: false, revision: 2, message: 'Server startup failed.', rollback: 'restored'}}));
    assert.match(f.doc.querySelector('#runtime-operation').textContent, /Previous instrument restored/);
    assert.equal(f.doc.querySelector('#runtime-operation').dataset.state, 'error');
    assert.equal(f.doc.querySelector('#upload-btn').disabled, false);
});

test('preflight safely renders metadata and default selection uses POST', async t => {
    const f = fixture(t); f.sockets[0].reply(status());
    const scope = f.doc.querySelector('tr[data-name="scope"]');
    Array.from(scope.querySelectorAll('button')).find(b => b.textContent === 'Check instrument').click();
    f.requests.shift().reply(200, {ready: false, message: 'Wrong board', metadata: {board: '<img src=x>'}, warnings: ['Unverified'], archive: {extracted_bytes: 100}});
    await f.flush();
    assert.match(f.doc.querySelector('#preflight-content').textContent, /Wrong board/);
    assert.equal(f.doc.querySelector('#preflight-content img'), null);
    Array.from(scope.querySelectorAll('button')).find(b => b.textContent === 'Start at boot').click();
    const request = f.requests.shift(); assert.equal(request.method, 'POST'); assert.match(request.url, /default\/scope$/);
    request.reply(422, {error: 'Wrong board'}); await f.flush();
    assert.match(f.doc.querySelector('#upload-status').textContent, /Wrong board/);
});

test('lost WebSocket falls back to HTTP, reconnects and disposes timers', async t => {
    const f = fixture(t); f.sockets[0].reply(status()); f.sockets[0].onclose();
    assert.equal(f.requests.length, 1); assert.match(f.requests[0].url, /system\/status$/);
    f.requests.shift().reply(200, status()); await f.flush();
    const retry = Array.from(f.timers.values()).find(timer => timer.delay === 1000);
    assert.ok(retry); retry.fn(); assert.equal(f.sockets.length, 2);
    f.sockets[1].reply(status());
    f.w.runtime.dispose(); assert.equal(f.sockets[1].closed, true);
    // Our deterministic timer mock retains fired callbacks; dispose removes live timers.
    assert.equal(Array.from(f.timers.values()).filter(timer => timer.delay === 2000 || timer.delay === 7000).length, 0);
});

test('unavailable health values remain explicit and stale data is marked', async t => {
    const f = fixture(t); f.sockets[0].reply(status({health: {memory: {}, storage: {}}}));
    assert.match(f.doc.querySelector('#health-table').textContent, /Unavailable/);
    f.sockets[0].onclose(); f.requests.shift().onerror(); await f.flush();
    assert.equal(f.doc.querySelector('#health-table').dataset.stale, 'true');
    assert.equal(f.doc.querySelector('#instrument-stop').disabled, true);
});

test('late HTTP snapshots and errors cannot overwrite a newer WebSocket state', async t => {
    const f = fixture(t); f.sockets[0].reply(status());
    void f.w.runtime.refresh(); const older = f.requests.shift();
    f.sockets[0].reply(status({operation: {phase: 'starting', busy: true, revision: 2}}));
    older.reply(200, status()); await f.flush();
    assert.equal(f.doc.querySelector('#instrument-stop').disabled, true);
    assert.match(f.doc.querySelector('#runtime-operation').textContent, /Starting/);
    void f.w.runtime.refresh(); const failed = f.requests.shift();
    f.sockets[0].reply(status({operation: {phase: 'succeeded', busy: false, revision: 3, message: 'Started'}}));
    failed.onerror(); await f.flush();
    assert.equal(f.doc.querySelector('#board-connection').textContent, 'Connected');
    assert.equal(f.doc.querySelector('#health-table').dataset.stale, 'false');
});

test('disconnect marks controls stale immediately and status reads time out promptly', t => {
    const f = fixture(t);
    assert.equal(f.doc.querySelector('#upload-btn').disabled, true);
    f.sockets[0].reply(status()); f.sockets[0].onclose();
    assert.equal(f.doc.querySelector('#instrument-stop').disabled, true);
    assert.equal(f.doc.querySelector('#health-table').dataset.stale, 'true');
    assert.equal(f.requests[0].timeout, 8000);
});

test('dispose cancels in-flight status and ignores its late result', async t => {
    const f = fixture(t); f.sockets[0].reply(status());
    void f.w.runtime.refresh(); const request = f.requests.shift();
    f.w.runtime.dispose(); assert.equal(request.aborted, true);
    request.reply(200, status({current_instrument: installed[1], instruments: {instruments: installed, live_instrument: installed[1]}}));
    await f.flush();
    assert.equal(f.doc.querySelector('#live-name').textContent, 'fft');
    assert.equal(Array.from(f.timers.values()).filter(timer => timer.delay === 2000).length, 0);
});

test('malformed nested status is rejected without rendering or enabling commands', t => {
    const f = fixture(t); f.sockets[0].reply(status());
    f.sockets[0].reply(status({operation: {phase: 'idle', busy: 'false', revision: 0},
        instruments: {instruments: [{name: 5}], live_instrument: null}}));
    assert.equal(f.doc.querySelector('#upload-btn').disabled, true);
    assert.equal(f.doc.querySelector('tr[data-name="5"]'), null);
    assert.equal(f.sockets[0].closed, true);
});

test('blocked WebSocket constructor falls back to HTTP', async t => {
    const f = fixture(t); f.w.runtime.dispose();
    f.w.WebSocket = class { constructor() { throw new f.w.DOMException('Blocked', 'SecurityError'); } };
    assert.doesNotThrow(() => f.w.eval('window.runtime = new RuntimeStream(document);'));
    assert.equal(f.requests.length, 1);
    f.requests.shift().reply(200, status()); await f.flush();
    assert.ok(Array.from(f.timers.values()).some(timer => timer.delay === 2000));
    f.w.runtime.dispose();
});

test('hidden pages cancel old reads and resume with a fresh stream', async t => {
    const f = fixture(t); f.sockets[0].reply(status());
    void f.w.runtime.refresh(); const old = f.requests.shift();
    Object.defineProperty(f.doc, 'hidden', {configurable: true, value: true});
    f.doc.dispatchEvent(new f.w.Event('visibilitychange'));
    assert.equal(old.aborted, true); assert.equal(f.sockets[0].closed, true);
    Object.defineProperty(f.doc, 'hidden', {configurable: true, value: false});
    f.doc.dispatchEvent(new f.w.Event('visibilitychange'));
    assert.equal(f.sockets.length, 2);
    f.sockets[1].reply(status({current_instrument: installed[1],
        instruments: {instruments: installed, live_instrument: installed[1]}}));
    old.reply(200, status()); await f.flush();
    assert.equal(f.doc.querySelector('#live-name').textContent, 'scope');
});

test('timed-out fallback recovers instead of holding the read lock', async t => {
    const f = fixture(t); f.sockets[0].reply(status()); f.sockets[0].onclose();
    f.requests.shift().ontimeout(); await f.flush();
    assert.equal(f.doc.querySelector('#upload-btn').disabled, true);
    const poll = Array.from(f.timers.values()).find(timer => timer.delay === 2000);
    assert.ok(poll); poll.fn();
    assert.equal(f.requests.length, 1); f.requests.shift().reply(200, status()); await f.flush();
    assert.equal(f.doc.querySelector('#upload-btn').disabled, false);
    assert.equal(f.doc.querySelector('#health-table').dataset.stale, 'false');
});

test('initial connection is neutral and manual retry remains available after a failure', async t => {
    const f = fixture(t);
    assert.equal(f.doc.querySelector('#board-connection').textContent, 'Connecting…');
    assert.equal(f.doc.querySelector('#instruments-status').dataset.state, 'loading');
    assert.equal(f.doc.querySelector('#refresh-instruments').disabled, false);
    f.sockets[0].onclose(); f.requests.shift().onerror(); await f.flush();
    assert.equal(f.doc.querySelector('#refresh-instruments').textContent, 'Retry');
    assert.equal(f.doc.querySelector('#refresh-instruments').disabled, false);
    f.doc.querySelector('#refresh-instruments').click();
    f.requests.shift().reply(200, status()); await f.flush();
    assert.equal(f.doc.querySelector('#refresh-instruments').textContent, 'Refresh');
    assert.equal(f.doc.querySelector('#upload-btn').disabled, false);
});

test('pending lifecycle feedback survives an older idle or completed snapshot', async t => {
    const f = fixture(t); f.sockets[0].reply(status());
    f.doc.querySelector('#instrument-stop').click();
    f.sockets[0].reply(status());
    assert.equal(f.doc.querySelector('#runtime-operation').hidden, false);
    assert.equal(f.doc.querySelector('#runtime-operation').textContent, 'Stopping instrument…');
    f.sockets[0].reply(status({operation: {phase: 'succeeded', action: 'start', busy: false, revision: 0}}));
    assert.equal(f.doc.querySelector('#runtime-operation').textContent, 'Stopping instrument…');
    f.requests.shift().reply(200, {message: 'Instrument stop completed'}); await f.flush();
    f.sockets[0].reply(status({operation: {phase: 'succeeded', action: 'stop', busy: false, revision: 2}}));
    assert.equal(f.doc.querySelector('#runtime-operation').textContent, 'Instrument stopped.');
    assert.equal(f.doc.querySelector('#upload-status').hidden, true);
});

test('action menus close on outside clicks and Escape returns keyboard focus', t => {
    const f = fixture(t); f.sockets[0].reply(status());
    const menus = f.doc.querySelectorAll('.instrument-options');
    menus[0].querySelector('summary').click(); assert.equal(menus[0].open, true);
    menus[1].querySelector('summary').click();
    assert.equal(menus[0].open, false); assert.equal(menus[1].open, true);
    menus[1].querySelector('button').focus();
    f.doc.dispatchEvent(new f.w.KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
    assert.equal(menus[1].open, false);
    assert.equal(f.doc.activeElement, menus[1].querySelector('summary'));
    menus[1].querySelector('summary').click(); f.doc.querySelector('#instruments-heading').click();
    assert.equal(menus[1].open, false);
});

test('instrument check moves focus into its panel and restores the originating action', async t => {
    const f = fixture(t); f.sockets[0].reply(status());
    const menu = f.doc.querySelector('tr[data-name="scope"] .instrument-options');
    menu.querySelector('summary').click(); menu.querySelector('button').click();
    assert.equal(menu.open, false);
    assert.equal(f.doc.activeElement, f.doc.querySelector('#preflight-close'));
    f.doc.querySelector('#preflight-close').click();
    assert.equal(f.doc.activeElement, menu.querySelector('summary'));
    f.requests.shift().reply(200, {ready: true}); await f.flush();
    assert.equal(f.doc.querySelector('#preflight-panel').hidden, true);
    assert.equal(f.doc.querySelector('#preflight-content').textContent, 'Checking instrument…');
});

test('health displays short uptime and startup durations without rounding them to zero', t => {
    const f = fixture(t); f.sockets[0].reply(status({health: {uptime_seconds: 37,
        timing: {api_initialization_us: 12, extraction: {ExecMainStartTimestampMonotonic: 100, ExecMainExitTimestampMonotonic: 2100}},
        instrument_service: {ExecMainStartTimestampMonotonic: 100, ActiveEnterTimestampMonotonic: 75100}}}));
    const values = Object.fromEntries(Array.from(f.doc.querySelectorAll('#health-table tr'), row => [row.cells[0].textContent, row.cells[1].textContent]));
    assert.equal(values.Uptime, '37 s');
    assert.equal(values['API initialization'], '<0.1 ms');
    assert.equal(values['Server startup'], '75.0 ms');
    assert.equal(values['Boot extraction'], '2.0 ms');
});

test('small extracted sizes stay visible and full versions remain in accessible text', t => {
    const f = fixture(t); f.sockets[0].reply(status({instruments: {instruments: [
        {name: 'scope', version: '1.0.0-development+red-pitaya', is_default: false}], live_instrument: null}}));
    const version = f.doc.querySelector('.instrument-version');
    assert.equal(version.textContent, '1.0.0-development+red-pitaya');
    assert.equal(version.title, version.textContent);
    assert.equal(f.w.PreflightView.bytes(120), '120 B');
    assert.equal(f.w.PreflightView.bytes(0), '0 B');
    assert.equal(f.w.PreflightView.bytes(-1), 'Unavailable');
});

test('power and temperature share the status socket without a running instrument', t => {
    const f = fixture(t);
    const telemetry = {state: 'live', supplies: [
        {label: 'Input', power_w: 6, voltage_v: 12, current_a: .5},
        {label: 'Module', power_w: 1, voltage_v: 5, current_a: .2}],
        temperatures: [{label: 'Processor', celsius: 42.25}]};
    const update = () => f.sockets[0].reply(status({instruments: {instruments: installed, live_instrument: null},
        health: {telemetry}}));
    update();
    assert.equal(f.sockets.length, 1);
    assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Live');
    assert.match(f.doc.querySelector('#supply-table').textContent, /Input6.00 W12.00 V0.500 A/);
    assert.match(f.doc.querySelector('#temperature-table').textContent, /Processor42.3 °C/);
    const row = f.doc.querySelector('#supply-table tbody tr'); update();
    assert.equal(f.doc.querySelector('#supply-table tbody tr'), row);
    telemetry.temperatures[0].celsius = 43; update();
    assert.match(f.doc.querySelector('#temperature-table').textContent, /43.0 °C/);
});

test('missing sensor readings stay unavailable and labels are rendered as text', t => {
    const f = fixture(t);
    f.sockets[0].reply(status({health: {telemetry: {state: 'live',
        supplies: [{label: '<img src=x>', power_w: null, voltage_v: '12', current_a: false}],
        temperatures: [{label: 'Sensor', celsius: null}]}}}));
    assert.equal(f.doc.querySelector('#supply-table tbody').textContent, '<img src=x>———');
    assert.equal(f.doc.querySelector('#supply-table img'), null);
    assert.equal(f.doc.querySelector('#temperature-table tbody').textContent, 'Sensor—');
    assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Sensor readings unavailable');
    f.sockets[0].reply(status({health: {}}));
    assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Telemetry unavailable');
    assert.equal(f.doc.querySelector('#supply-table tbody').children.length, 0);
});

test('stale sensors and disconnected transport never claim live readings', t => {
    const f = fixture(t);
    f.sockets[0].reply(status({health: {telemetry: {state: 'stale', supplies: [],
        temperatures: [{label: 'Processor', celsius: 42}]}}}));
    assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Stale · last known readings');
    assert.equal(f.doc.querySelector('#temperature-table').dataset.stale, 'true');
    assert.match(f.doc.querySelector('#temperature-table').textContent, /42.0 °C/);
    f.sockets[0].onclose();
    assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Disconnected · last known readings');
    f.requests.shift().reply(200, status({health: {telemetry: {state: 'live', supplies: [],
        temperatures: [{label: 'Processor', celsius: 43}]}}}));
    return f.flush().then(() => {
        assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Live');
        assert.equal(f.doc.querySelector('#temperature-table').dataset.stale, 'false');
    });
});

test('voltage-only sensors have their own table and survive stale and failed reads', t => {
    const f = fixture(t);
    const telemetry = {state: 'live', supplies: [], temperatures: [], voltages: [
        {label: 'Core', voltage_v: .85}, {label: 'Auxiliary', voltage_v: null}]};
    const update = () => f.sockets[0].reply(status({health: {telemetry}}));
    update();
    assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Live');
    assert.equal(f.doc.querySelector('#voltage-details').hidden, false);
    assert.equal(f.doc.querySelector('#voltage-table').textContent, 'Additional supply voltage measurementsCore0.850 VAuxiliary—');
    assert.equal(f.doc.querySelector('#supply-table').hidden, true);
    const details = f.doc.querySelector('#voltage-details'); details.open = true;
    telemetry.voltages[0].voltage_v = .86; update();
    assert.equal(details.open, true);
    assert.match(f.doc.querySelector('#voltage-table').textContent, /0.860 V/);
    telemetry.state = 'stale'; update();
    assert.equal(f.doc.querySelector('#voltage-table').dataset.stale, 'true');
    assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Stale · last known readings');
    f.sockets[0].onclose();
    assert.equal(f.doc.querySelector('#voltage-table').dataset.stale, 'true');
    f.w.runtime.dispose();
});

test('hardware statuses remain explicit across warnings, stale data and disconnects', t => {
    const f = fixture(t);
    const telemetry = {state: 'live', statuses: [
        {label: 'Clock', state: 'ok', message: 'Locked'},
        {label: 'History', state: 'warning', message: 'Previous unlock'},
        {label: '<img src=x>', state: 'fault', message: '<b>Fault</b>'},
        {label: 'Sensor', state: 'unavailable'}]};
    const update = () => f.sockets[0].reply(status({health: {telemetry}}));
    update();
    const table = f.doc.querySelector('#sensor-status-table');
    assert.equal(f.doc.querySelector('#telemetry-status').textContent, 'Live');
    assert.equal(table.hidden, false);
    assert.equal(table.tBodies[0].rows[0].textContent, 'ClockOK · Locked');
    assert.equal(table.tBodies[0].rows[1].cells[1].dataset.state, 'warning');
    assert.equal(table.tBodies[0].rows[2].cells[1].dataset.state, 'error');
    assert.equal(table.tBodies[0].rows[3].textContent, 'SensorUnavailable');
    assert.equal(table.querySelector('img, b'), null);
    const row = table.tBodies[0].rows[0]; update();
    assert.equal(table.tBodies[0].rows[0], row);
    telemetry.state = 'stale'; update(); assert.equal(table.dataset.stale, 'true');
    f.sockets[0].onclose(); assert.equal(table.dataset.stale, 'true');
    f.requests.shift().reply(200, status({health: {}}));
    return f.flush().then(() => assert.equal(table.hidden, true));
});

test('CPU policies distinguish hardware and requested rates and recover after disconnect', t => {
    const f = fixture(t);
    const cpu = {policies: [
        {label: 'policy0', affected_cpus: '0 1', frequency_hz: 1200000000, frequency_source: 'hardware', governor: 'schedutil'},
        {label: 'policy2', frequency_hz: 800000000, frequency_source: 'requested', governor: '<b>performance</b>'},
        {label: 'policy4', frequency_hz: null}]};
    f.sockets[0].reply(status({health: {cpu}}));
    const table = f.doc.querySelector('#cpu-table');
    assert.match(table.textContent, /CPU 0 11200 MHz \(hardware\) · schedutil/);
    assert.match(table.textContent, /800 MHz \(requested\)/);
    assert.match(table.textContent, /CPU policy4Unavailable/);
    assert.equal(table.querySelector('b'), null);
    f.sockets[0].onclose(); assert.equal(table.dataset.stale, 'true');
    f.requests.shift().reply(200, status({health: {cpu: {policies: []}}}));
    return f.flush().then(() => {
        assert.equal(table.dataset.stale, 'false');
        assert.match(table.textContent, /CPU frequencyUnavailable/);
    });
});
