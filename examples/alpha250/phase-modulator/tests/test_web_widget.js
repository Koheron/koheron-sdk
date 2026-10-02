// npm install --prefix web; NODE_PATH=web/node_modules node --test this_file
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');

const root = path.resolve(__dirname, '../../../..');
function environment(t) {
    const dom = new JSDOM('<div id="first"></div><div id="second"></div>', {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    for (const file of ['web/koheron.ts', 'web/phase-modulator/phase-modulator.ts', 'web/phase-modulator/phase-modulator-widget.ts']) {
        dom.window.eval(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {
            compilerOptions: {target: ts.ScriptTarget.ES2020}
        }).outputText + '\nwindow.TestDriver = typeof PhaseModulatorDriver !== "undefined" ? PhaseModulatorDriver : window.TestDriver;' +
            '\nwindow.TestWidget = typeof PhaseModulatorWidget !== "undefined" ? PhaseModulatorWidget : window.TestWidget;' +
            '\nwindow.TestClient = typeof Client !== "undefined" ? Client : window.TestClient;');
    }
    return dom.window;
}
function port(count = 2, capabilities = 1023) {
    const values = Array.from({length: count}, (_, channel) => ({carrier: 10e6, phase: 17, modulation: 10e3,
        deviation: 1, duty: .5, seed: 1, waveform: 0, output: channel === 0, pm: true}));
    return {
        values, calls: [],
        async init() { return values.map(() => ({sampleRate: 250e6, phaseWidth: 48, prbsWidth: 7, capabilities})); },
        async settings(channel) { return {...values[channel]}; },
        async set(channel, field, value) { this.calls.push([channel, field, value]); values[channel][field] = value; },
        async restart(channel) { this.calls.push([channel, 'restart']); }
    };
}
function change(window, root, channel, field, value) {
    const input = root.querySelector(`[data-channel="${channel}"] [data-field="${field}"]`);
    if (input.type === 'checkbox') { input.checked = value; } else { input.value = String(value); }
    input.dispatchEvent(new window.Event('change', {bubbles: true}));
    return input;
}
const settle = () => new Promise(resolve => setImmediate(resolve));

test('opening reads current hardware without writes; widget instances are isolated', async t => {
    const window = environment(t);
    const first = window.document.getElementById('first');
    const second = window.document.getElementById('second');
    const a = port(); const b = port(1);
    const widget = new window.TestWidget(first, a);
    await widget.init();
    await new window.TestWidget(second, b).init();
    assert.equal(first.querySelector('[data-field="carrier"]').value, '10');
    assert.equal(first.querySelector('[data-field="modulation"]').value, '10');
    assert.equal(first.querySelector('[data-field="deviation"]').value, '1');
    assert.equal(first.querySelectorAll('.pm-channel').length, 2);
    assert.equal(second.querySelectorAll('.pm-channel').length, 1);
    assert.equal(a.calls.length + b.calls.length, 0);
    change(window, first, 0, 'carrier', 12);
    await settle();
    assert.deepEqual(a.calls, [[0, 'carrier', 12e6]]);
    assert.equal(b.calls.length, 0);
    assert.equal(second.querySelector('[data-field="carrier"]').value, '10');
    assert.equal(a.values[0].phase, 17);
    assert.equal(a.values[1].carrier, 10e6);
    widget.dispose();
    change(window, first, 0, 'carrier', 14);
    await settle();
    assert.equal(a.calls.length, 1);
});

test('number edits validate before commands; typing alone does not commit', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    await new window.TestWidget(target, driver).init();
    for (const value of ['', '-1', '125', '126', 'NaN', 'Infinity']) {
        change(window, target, 0, 'carrier', value);
        await settle();
    }
    for (const value of ['', '-1', '361']) { change(window, target, 0, 'deviation', value); }
    assert.equal(driver.calls.length, 0);
    const input = target.querySelector('[data-field="carrier"]');
    input.value = '11'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    assert.equal(driver.calls.length, 0);
    input.dispatchEvent(new window.Event('change', {bubbles: true}));
    await settle();
    assert.deepEqual(driver.calls, [[0, 'carrier', 11e6]]);
    assert.equal(target.querySelector('.pm-status').hidden, true);
});

test('display hides DDS quantization artifacts but retains tiny phase settings', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    const turn = 2 ** 48;
    driver.values[0].carrier = Math.round(10e6 / 250e6 * turn) / turn * 250e6;
    driver.values[0].modulation = Math.round(10e3 / 250e6 * turn) / turn * 250e6;
    driver.values[0].deviation = Math.round(turn / 360) / turn * 360;
    driver.values[0].phase = 360 / turn;
    await new window.TestWidget(target, driver).init();
    assert.equal(target.querySelector('[data-field="carrier"]').value, '10');
    assert.equal(target.querySelector('[data-field="modulation"]').value, '10');
    assert.equal(target.querySelector('[data-field="deviation"]').value, '1');
    const tiny = Number(target.querySelector('[data-field="phase"]').value);
    assert(tiny > 0 && Math.abs(tiny - 360 / turn) <= 180 / turn);
    assert.equal(driver.calls.length, 0);
});

test('mute/resume, PM, source, duty, seed and restart use only their intended operation', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    await new window.TestWidget(target, driver).init();
    const before = {...driver.values[0]};
    target.querySelector('[data-action="output"]').click(); await settle();
    assert.equal(driver.values[0].output, false);
    assert.equal(target.querySelector('[data-action="output"]').textContent, 'Muted');
    target.querySelector('[data-action="output"]').click(); await settle();
    assert.deepEqual(driver.values[0], before);
    change(window, target, 0, 'pm', false); await settle();
    assert.equal(target.querySelector('[data-field="modulation"]').disabled, false); // Prepare PM while off.
    change(window, target, 0, 'waveform', 2); await settle();
    assert.equal(target.querySelector('[data-field="duty"]').disabled, false);
    change(window, target, 0, 'duty', 25); await settle();
    assert.equal(driver.values[0].duty, .25);
    change(window, target, 0, 'waveform', 8); await settle();
    change(window, target, 0, 'seed', 128); await settle();
    assert.equal(driver.values[0].seed, 1); // PN7 zero seed is rejected locally.
    change(window, target, 0, 'seed', 13); await settle();
    target.querySelector('[data-action="more"]').click();
    assert.equal(target.querySelector('.pm-advanced').hidden, false);
    target.querySelector('[data-action="restart"]').click(); await settle();
    assert.deepEqual(driver.calls.at(-1), [0, 'restart']);
    assert.equal(driver.values[0].phase, 17);
    assert.equal(driver.values[0].seed, 13);
});

test('pending commands lock only their channel; failures show accepted hardware state', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port();
    await new window.TestWidget(target, driver).init();
    let release;
    driver.set = async (channel, field, value) => {
        driver.calls.push([channel, field, value]);
        await new Promise(resolve => { release = resolve; });
        throw new Error('Commit acknowledgement timed out');
    };
    const input = target.querySelector('[data-field="carrier"]');
    input.focus();
    change(window, target, 0, 'carrier', 12);
    assert.equal(target.querySelector('[data-channel="0"] fieldset').getAttribute('aria-busy'), 'true');
    assert.equal(input.readOnly, true);
    assert.equal(window.document.activeElement, input); // Pending commits retain keyboard focus.
    assert.equal(target.querySelector('[data-channel="1"] fieldset').disabled, false);
    assert.equal(target.querySelector('[data-channel="1"] [data-field="carrier"]').readOnly, false);
    change(window, target, 0, 'pm', false); // A second edit cannot overtake the pending one.
    assert.equal(driver.calls.length, 1);
    release(); await settle();
    assert.equal(target.querySelector('[data-field="carrier"]').value, '10');
    assert.match(target.querySelector('.pm-status').textContent, /timed out/);
    assert.equal(target.querySelector('[data-channel="0"] fieldset').disabled, false);
    assert.equal(input.readOnly, false);
    target.querySelector('[data-action="refresh"]').click(); await settle();
    assert.equal(target.querySelector('.pm-status').hidden, true);
    assert.equal(driver.calls.length, 1); // Refresh never reissues the failed command.
});

test('reduced source capabilities and initialization retry', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1, 2);
    driver.values[0].pm = false;
    let fail = true;
    const init = driver.init;
    driver.init = async () => { if (fail) { throw new Error('Previous commit is still pending'); } return init(); };
    const widget = new window.TestWidget(target, driver);
    await assert.rejects(widget.init(), /still pending/);
    assert.match(target.querySelector('[role="alert"]').textContent, /still pending/);
    fail = false; target.querySelector('[data-action="retry"]').click(); await settle();
    assert.equal(target.querySelectorAll('.pm-channel').length, 1);
    const options = [...target.querySelector('[data-field="waveform"]').options];
    assert.deepEqual(options.filter(option => !option.disabled).map(option => option.textContent), ['Square']);
    assert.equal(driver.calls.length, 0);
});

test('actual SDK tuple decoder preserves all 48-bit words and captures readback errors', async t => {
    const window = environment(t);
    const descriptions = {
        get_channel_count: [], get_sample_rate: [], get_initialization_error: [],
        get_channel_info: ['uint32_t'], get_settings_words: ['uint32_t'], get_error_message: ['uint32_t'],
        set_carrier_frequency: ['uint32_t', 'double']
    };
    const commands = Object.fromEntries(Object.entries(descriptions).map(([name, types], id) =>
        [name, {name, id, args: types.map(type => ({type}))}]));
    const wire = new window.DataView(new window.ArrayBuffer(62));
    const native = [Math.round(10e6 / 250e6 * 2 ** 48), 1, Math.round(10e3 / 250e6 * 2 ** 48), 3, 17, 2 ** 48];
    native.forEach((word, index) => {
        wire.setUint32(4 + index * 8, Math.floor(word / 2 ** 32));
        wire.setUint32(8 + index * 8, word % 2 ** 32);
    });
    wire.setUint32(52, 1); wire.setUint32(56, 0); wire.setUint8(60, 1); wire.setUint8(61, 1);
    const calls = [];
    const client = {
        getDriver() { return {id: 12, getCmds: () => commands}; },
        async readUint32(command) { return command.cmd.name === 'get_channel_count' ? 1 : 250e6; },
        async readTuple(command, fmt) {
            if (command.cmd.name === 'get_channel_info') { return [48, 24, 14, 7, 16, 1023]; }
            return window.TestClient.prototype.deserialize(fmt, wire);
        },
        async readString(command) { calls.push(command); return command.cmd.name === 'get_error_message' ? 'Sample clock stopped' : ''; }
    };
    const driver = new window.TestDriver(client);
    await driver.init();
    const settings = await driver.settings(0);
    assert.equal(settings.phase, 360 / 2 ** 48);
    assert.equal(settings.deviation, 17 * 360 / 2 ** 48);
    assert.equal(settings.duty, 1);
    assert.equal(settings.output, true);
    await driver.set(0, 'carrier', 12e6);
    assert.equal(calls[0].cmd.name, 'set_carrier_frequency');
    const request = new window.DataView(calls[0].data.buffer);
    assert.equal(request.getUint32(8), 0);
    assert.equal(request.getFloat64(12), 12e6);
    wire.setUint32(0, 17);
    await assert.rejects(driver.settings(0), /Sample clock stopped/);
    assert.equal(new window.DataView(calls.at(-1).data.buffer).getUint32(8), 17);
});

test('SDK string decoder accepts empty success responses and respects payload views', async t => {
    const window = environment(t);
    const client = Object.create(window.TestClient.prototype);
    assert.equal(client.parseString(new window.DataView(new window.ArrayBuffer(0))), '');
    const bytes = new window.Uint8Array([255, 255, 79, 75, 255]);
    const view = new window.DataView(bytes.buffer, 2, 2);
    assert.equal(client.parseString(view), 'OK');
    assert.equal(client.parseString(view, 2), '');
    client._readBaseAsync = async () => new window.DataView(new window.ArrayBuffer(0));
    assert.equal(await client.readString({}), ''); // Exercise the real checked-RPC path.
});
