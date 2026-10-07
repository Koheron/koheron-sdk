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
    for (const file of ['web/koheron.ts', 'web/inputs/digit-input.ts', 'web/phase-modulator/phase-modulator.ts', 'web/phase-modulator/phase-modulator-widget.ts']) {
        dom.window.eval(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {
            compilerOptions: {target: ts.ScriptTarget.ES2020}
        }).outputText + '\nwindow.TestDriver = typeof PhaseModulatorDriver !== "undefined" ? PhaseModulatorDriver : window.TestDriver;' +
            '\nwindow.TestWidget = typeof PhaseModulatorWidget !== "undefined" ? PhaseModulatorWidget : window.TestWidget;' +
            '\nif (typeof FrequencyInput !== "undefined") { window.FrequencyInput = FrequencyInput; window.NumberInput = NumberInput; }' +
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
const settle = () => new Promise(resolve => setTimeout(resolve, 20));

test('opening reads current hardware without writes; widget instances are isolated', async t => {
    const window = environment(t);
    const first = window.document.getElementById('first');
    const second = window.document.getElementById('second');
    const a = port(); const b = port(1);
    const widget = new window.TestWidget(first, a);
    await widget.init();
    await new window.TestWidget(second, b).init();
    assert.equal(first.querySelector('[data-field="carrier"]').value, '10.000\u2009000');
    assert.equal(first.querySelector('[data-field="modulation"]').value, '10.000');
    assert.equal(first.querySelector('[data-field="deviation"]').value, '1');
    assert.equal(first.querySelectorAll('.pm-channel').length, 2);
    assert.equal(second.querySelectorAll('.pm-channel').length, 1);
    assert.equal(a.calls.length + b.calls.length, 0);
    change(window, first, 0, 'carrier', 12);
    await settle();
    assert.deepEqual(a.calls, [[0, 'carrier', 12e6]]);
    assert.equal(b.calls.length, 0);
    assert.equal(second.querySelector('[data-field="carrier"]').value, '10.000\u2009000');
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
    assert.match(target.querySelector('.pm-status').textContent, /PM depth:/); // An unrelated acknowledgement preserves invalid entry.
    target.querySelector('[data-field="deviation"]').dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
    assert.equal(target.querySelector('.pm-status').hidden, true);
});

test('angle entry applies once on Enter, keeps focus and cancels with Escape', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    await new window.TestWidget(target, driver).init();
    const input = target.querySelector('[data-field="deviation"]');
    input.focus(); input.value = '2.5';
    input.dispatchEvent(new window.Event('input', {bubbles: true}));
    assert.equal(driver.calls.length, 0);
    input.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    input.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    await settle();
    assert.deepEqual(driver.calls, [[0, 'deviation', 2.5]]);
    assert.equal(window.document.activeElement, input);
    input.dispatchEvent(new window.Event('change', {bubbles: true})); // Native change can follow Enter on blur.
    await settle(); assert.equal(driver.calls.length, 1);
    input.value = '3'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    input.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
    assert.equal(input.value, '2.5');
    assert.equal(window.document.activeElement, input);
    input.dispatchEvent(new window.Event('change', {bubbles: true}));
    await settle(); assert.equal(driver.calls.length, 1);
    input.value = '3'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    input.blur(); await settle();
    assert.deepEqual(driver.calls, [[0, 'deviation', 2.5], [0, 'deviation', 3]]);
});

test('readbacks preserve numeric drafts and their validation until correction or Escape', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    await new window.TestWidget(target, driver).init();
    const input = target.querySelector('[data-field="deviation"]');
    input.value = '2.75'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    target.querySelector('[data-action="refresh"]').click(); await settle();
    assert.equal(input.value, '2.75');
    assert.equal(driver.values[0].deviation, 1);
    input.value = '361'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    input.dispatchEvent(new window.Event('change', {bubbles: true}));
    target.querySelector('[data-action="output"]').click(); await settle();
    assert.equal(input.value, '361');
    assert.equal(input.getAttribute('aria-invalid'), 'true');
    assert(input.getAttribute('aria-description'));
    assert.match(target.querySelector('.pm-status').textContent, /PM depth:/);
    assert.equal(target.querySelector('.pm-status').getAttribute('role'), 'alert');
    input.value = '2'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    assert.equal(input.hasAttribute('aria-invalid'), false);
    assert.equal(target.querySelector('.pm-status').hidden, true);
    input.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
    assert.equal(input.value, '1');
    assert.deepEqual(driver.calls, [[0, 'output', false]]);
    input.value = '361'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    input.dispatchEvent(new window.Event('change', {bubbles: true}));
    driver.set = async () => { throw new Error('Sample clock stopped'); };
    target.querySelector('[data-action="output"]').click(); await settle();
    assert.match(target.querySelector('.pm-status').textContent, /Sample clock stopped.*PM depth:/);
    input.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
    assert.match(target.querySelector('.pm-status').textContent, /^(?:Error: )?Sample clock stopped$/);
});

test('committing an existing numeric draft waits behind the channel operation', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    await new window.TestWidget(target, driver).init();
    const input = target.querySelector('[data-field="deviation"]');
    input.value = '2'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    let release;
    driver.set = async (channel, field, value) => {
        driver.calls.push([channel, field, value]);
        if (field === 'output') { await new Promise(resolve => { release = resolve; }); }
        driver.values[channel][field] = value;
    };
    target.querySelector('[data-action="output"]').click();
    input.dispatchEvent(new window.Event('change', {bubbles: true}));
    input.dispatchEvent(new window.Event('change', {bubbles: true}));
    assert.equal(driver.calls.length, 1);
    assert.equal(input.value, '2');
    release(); await settle();
    assert.deepEqual(driver.calls, [[0, 'output', false], [0, 'deviation', 2]]);
    assert.equal(driver.values[0].deviation, 2);
    assert.equal(input.value, '2');
});

test('frequency validation stays visible after blur and unrelated readbacks', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    await new window.TestWidget(target, driver).init();
    const input = target.querySelector('[data-field="carrier"]');
    input.focus(); input.value = '125 MHz'; input.dispatchEvent(new window.Event('input', {bubbles: true}));
    target.querySelector('[data-field="deviation"]').focus();
    await settle();
    const status = target.querySelector('.pm-status');
    assert.match(status.textContent, /Carrier:.*below 125 MHz/);
    assert.equal(status.hidden, false);
    assert.match(input.getAttribute('aria-description'), /below 125 MHz/);
    target.querySelector('[data-action="refresh"]').click(); await settle();
    assert.match(status.textContent, /Carrier:.*below 125 MHz/);
    assert.equal(input.value, '125 MHz');
    input.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Escape', bubbles: true}));
    assert.equal(status.hidden, true);
    assert.equal(input.value, '10.000\u2009000');
    assert.equal(driver.calls.length, 0);
});

test('display hides DDS quantization artifacts but retains tiny phase settings', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    const turn = 2 ** 48;
    driver.values[0].carrier = Math.round(10e6 / 250e6 * turn) / turn * 250e6;
    driver.values[0].modulation = Math.round(10e3 / 250e6 * turn) / turn * 250e6;
    driver.values[0].deviation = Math.round(turn / 360) / turn * 360;
    driver.values[0].phase = 360 / turn;
    await new window.TestWidget(target, driver).init();
    assert.equal(target.querySelector('[data-field="carrier"]').value, '10.000\u2009000');
    assert.equal(target.querySelector('[data-field="modulation"]').value, '10.000');
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
    await settle();
    assert.equal(target.querySelector('[data-channel="0"] fieldset').getAttribute('aria-busy'), 'true');
    assert.equal(target.querySelector('[data-field="phase"]').readOnly, false); // Every numeric tuner can queue behind the channel operation.
    assert.equal(input.readOnly, false); // Digit tuning can accumulate while a request is pending.
    assert.equal(window.document.activeElement, input); // Pending commits retain keyboard focus.
    assert.equal(target.querySelector('[data-channel="1"] fieldset').disabled, false);
    assert.equal(target.querySelector('[data-channel="1"] [data-field="carrier"]').readOnly, false);
    change(window, target, 0, 'pm', false); // A second edit cannot overtake the pending one.
    assert.equal(driver.calls.length, 1);
    release(); await settle();
    assert.equal(target.querySelector('[data-field="carrier"]').value, '10.000\u2009000');
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
    const widget = new window.TestWidget(target, driver, {expectedChannels: 1});
    assert.equal(target.querySelectorAll('.pm-skeleton .pm-channel').length, 1);
    await assert.rejects(widget.init(), /still pending/);
    assert.match(target.querySelector('[role="alert"]').textContent, /still pending/);
    assert.equal(target.querySelectorAll('.pm-skeleton .pm-channel').length, 1); // Failure retains the reserved layout.
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

function frequency(t, commit, value = 10e6) {
    const window = environment(t);
    const root = window.document.getElementById('first');
    root.innerHTML = '<span><input type="text"><select><option>Hz</option><option>kHz</option><option selected>MHz</option><option>GHz</option></select><span class="pm-frequency-help"></span></span>';
    const input = root.querySelector('input'); const unit = root.querySelector('select');
    const calls = [];
    const control = new window.FrequencyInput(input, unit, {value, maximum: 125e6, resolution: 250e6 / 2 ** 48,
        commit: commit || (async value => { calls.push(value); return value; })});
    t.after(() => control.dispose());
    const key = (name, options = {}) => input.dispatchEvent(new window.KeyboardEvent('keydown', {key: name, bubbles: true, cancelable: true, ...options}));
    const digit = position => {
        input.focus(); input.setSelectionRange(position, position);
        input.dispatchEvent(new window.MouseEvent('click', {detail: 1, bubbles: true}));
    };
    const type = value => { input.value = value; input.dispatchEvent(new window.Event('input', {bubbles: true})); };
    return {window, input, unit, calls, control, key, digit, type};
}

test('direct frequency entry supports units/exponents, Enter, Escape and invalid ranges', async t => {
    const f = frequency(t);
    for (const [text, hz] of [['12.5 MHz', 12.5e6], ['2.5e4 Hz', 25e3], ['.01 GHz', 10e6], ['15k', 15e3]]) {
        f.type(text); assert.equal(f.calls.length, 0);
        f.key('Enter'); await settle();
        assert.deepEqual(f.calls.splice(0), [hz]);
    }
    for (const text of ['', 'NaN', 'Infinity', '-1 Hz', '125 MHz', '1000 GHz', '10 apples']) {
        f.type(text); f.key('Enter'); await settle();
        assert.equal(f.calls.length, 0); assert.equal(f.input.getAttribute('aria-invalid'), 'true');
    }
    f.type('25 MHz'); f.key('Escape'); await settle();
    assert.equal(f.calls.length, 0);
    assert.equal(Number(f.input.value.replace(/\s/g, '')) * 1e3, 15e3); // Last accepted value and unit.
    assert.equal(f.input.hasAttribute('aria-invalid'), false);
});

test('digit tuning preserves place value through carry/borrow and skips separators', async t => {
    const f = frequency(t, undefined, 9.999e6);
    f.digit(4); // 1 kHz place in 9.999 000 MHz.
    assert.equal(f.input.selectionStart, 4);
    assert.match(f.input.parentElement.querySelector('.pm-frequency-help').textContent, /Step 1 kHz/);
    f.key('ArrowUp'); await settle();
    assert.deepEqual(f.calls, [10e6]);
    assert.equal(f.input.value, '10.000\u2009000');
    assert.equal(f.input.selectionStart, 5); // Same physical step after another integer digit appears.
    f.key('ArrowRight');
    assert.equal(f.input.selectionStart, 7); // Skip the thin-space group separator.
    f.key('ArrowLeft');
    assert.equal(f.input.selectionStart, 5);
    f.key('ArrowDown'); await new Promise(resolve => setTimeout(resolve, 120));
    assert.equal(f.calls.at(-1), 9.999e6);
    assert.equal(f.input.selectionStart, 4);
    f.digit(0); f.key('ArrowRight');
    assert.equal(f.input.selectionStart, 2); // Skip the decimal point.
});

test('wheel follows the focused selected digit across the page, releases on blur and accumulates trackpad deltas', async t => {
    const f = frequency(t);
    const wheel = delta => {
        const event = new f.window.WheelEvent('wheel', {deltaY: delta, bubbles: true, cancelable: true});
        f.input.dispatchEvent(event); return event;
    };
    assert.equal(wheel(-100).defaultPrevented, false); // Hovering never changes hardware or captures page scroll.
    f.digit(5); // 1 kHz.
    for (let i = 0; i < 3; i++) { assert.equal(wheel(-10).defaultPrevented, true); }
    await settle(); assert.equal(f.calls.length, 0);
    wheel(-10); await settle();
    assert.deepEqual(f.calls, [10.001e6]);
    const elsewhere = f.window.document.getElementById('second');
    const pageWheel = () => {
        const event = new f.window.WheelEvent('wheel', {deltaY: -100, bubbles: true, cancelable: true});
        elsewhere.dispatchEvent(event); return event;
    };
    assert.equal(pageWheel().defaultPrevented, true);
    await new Promise(resolve => setTimeout(resolve, 120));
    assert.equal(f.calls.at(-1), 10.002e6);
    f.input.setSelectionRange(0, f.input.value.length);
    assert.equal(wheel(-100).defaultPrevented, false); // Ctrl+A is not a digit selection.
    f.digit(5);
    const zoom = new f.window.WheelEvent('wheel', {deltaY: -100, ctrlKey: true, cancelable: true});
    f.input.dispatchEvent(zoom); assert.equal(zoom.defaultPrevented, false);
    assert.equal(f.calls.length, 2);
    f.unit.focus();
    assert.equal(pageWheel().defaultPrevented, false);
    f.digit(5); f.control.dispose();
    assert.equal(pageWheel().defaultPrevented, false);
});

test('unit changes only reformat accepted values; unit selection finishes typed entry once', async t => {
    const f = frequency(t);
    f.unit.value = 'Hz'; f.unit.dispatchEvent(new f.window.Event('change'));
    await settle(); assert.equal(f.calls.length, 0);
    assert.equal(f.input.value, '10\u2009000\u2009000');
    f.input.focus(); f.type('20');
    f.input.dispatchEvent(new f.window.Event('change'));
    f.unit.focus(); // Typed number waits for the unit choice instead of briefly applying 20 Hz.
    await settle(); assert.equal(f.calls.length, 0);
    f.unit.value = 'kHz'; f.unit.dispatchEvent(new f.window.Event('change'));
    await settle(); assert.deepEqual(f.calls, [20e3]);
    f.input.dispatchEvent(new f.window.Event('change')); // A later native blur/change must not duplicate it.
    await settle(); assert.equal(f.calls.length, 1);
});

test('rapid tuning coalesces latest targets, retains selection and has no overlapping requests', async t => {
    let release; let active = 0; let maximum = 0; const calls = [];
    const f = frequency(t, async value => {
        calls.push(value); maximum = Math.max(maximum, ++active);
        await new Promise(resolve => { release = resolve; });
        active--; return value;
    });
    f.digit(5);
    f.key('ArrowUp'); f.key('ArrowUp'); f.key('ArrowUp');
    await settle(); assert.deepEqual(calls, [10.003e6]);
    f.key('ArrowUp'); f.key('ArrowUp');
    assert.equal(f.input.value, '10.005\u2009000');
    release(); await new Promise(resolve => setTimeout(resolve, 120));
    assert.deepEqual(calls, [10.003e6, 10.005e6]);
    release(); await settle();
    assert.equal(maximum, 1);
    assert.equal(f.input.value, '10.005\u2009000');
    assert.equal(f.input.selectionStart, 5);
});

test('failed digit commits cancel queued tuning; disposed controls issue no delayed writes', async t => {
    let reject; const calls = [];
    const f = frequency(t, async value => {
        calls.push(value); await new Promise((_, fail) => { reject = fail; }); return value;
    });
    f.digit(5); f.key('ArrowUp'); await settle();
    f.key('ArrowUp'); f.key('ArrowUp');
    reject(new Error('Commit acknowledgement timed out'));
    await new Promise(resolve => setTimeout(resolve, 120));
    assert.deepEqual(calls, [10.001e6]);
    assert.equal(f.input.value, '10.000\u2009000');
    assert.equal(f.input.getAttribute('aria-invalid'), 'true');
    f.key('ArrowUp'); f.control.dispose(); await new Promise(resolve => setTimeout(resolve, 120));
    assert.equal(calls.length, 1);
});

test('carrier and modulation edits queue coherently on one channel while preserving other settings', async t => {
    const window = environment(t); const target = window.document.getElementById('first'); const driver = port(1);
    let active = 0; let maximum = 0;
    const original = driver.set.bind(driver);
    driver.set = async (...args) => {
        maximum = Math.max(maximum, ++active);
        await new Promise(resolve => setTimeout(resolve, 25));
        await original(...args); active--;
    };
    await new window.TestWidget(target, driver).init();
    change(window, target, 0, 'carrier', '12 MHz');
    change(window, target, 0, 'modulation', '20 kHz');
    await new Promise(resolve => setTimeout(resolve, 100));
    assert.equal(maximum, 1);
    assert.deepEqual(driver.calls, [[0, 'carrier', 12e6], [0, 'modulation', 20e3]]);
    assert.equal(driver.values[0].carrier, 12e6);
    assert.equal(driver.values[0].modulation, 20e3);
    assert.equal(driver.values[0].phase, 17);
    assert.equal(driver.values[0].deviation, 1);
});

test('acknowledgements preserve whole-value selection; Escape exits wheel tuning even after late replies', async t => {
    let release;
    const f = frequency(t, value => new Promise(resolve => { release = () => resolve(value); }));
    const pageWheel = () => {
        const event = new f.window.WheelEvent('wheel', {deltaY: -100, bubbles: true, cancelable: true});
        f.window.document.getElementById('second').dispatchEvent(event);
        return event;
    };
    f.digit(5); f.key('ArrowUp'); await settle();
    f.key('a', {ctrlKey: true}); f.input.select();
    release(); await settle();
    assert.equal(f.input.selectionStart, 0);
    assert.equal(f.input.selectionEnd, f.input.value.length);
    assert.equal(pageWheel().defaultPrevented, false);

    f.digit(5); f.key('ArrowUp');
    await new Promise(resolve => setTimeout(resolve, 120));
    f.key('Escape');
    assert.equal(f.input.selectionStart, f.input.selectionEnd);
    assert.equal(pageWheel().defaultPrevented, false);
    release(); await settle(); // The already-issued command can complete.
    assert.equal(f.input.value, '10.002\u2009000');
    assert.equal(f.input.selectionStart, f.input.selectionEnd);
    assert.equal(pageWheel().defaultPrevented, false);

    f.key('F2');
    assert.equal(f.input.selectionStart, 0);
    assert.equal(f.input.selectionEnd, f.input.value.length);
    assert.equal(pageWheel().defaultPrevented, false);
    f.key('ArrowLeft');
    assert.equal(f.input.selectionEnd - f.input.selectionStart, 1);
    assert.equal(pageWheel().defaultPrevented, true);
});

test('trackpad accumulation resets between gestures and selected digits', async t => {
    const f = frequency(t);
    const wheel = () => f.input.dispatchEvent(new f.window.WheelEvent('wheel', {deltaY: -10, bubbles: true, cancelable: true}));
    f.digit(5);
    wheel(); wheel(); wheel();
    f.key('ArrowRight'); // Move from 1 kHz to 100 Hz; discard the unfinished gesture.
    wheel(); await settle(); assert.equal(f.calls.length, 0);
    await new Promise(resolve => setTimeout(resolve, 270));
    wheel(); wheel(); wheel(); await settle(); assert.equal(f.calls.length, 0);
    wheel(); await settle();
    assert.deepEqual(f.calls, [10.0001e6]);
});

test('all scalar settings use selected-digit tuning, including angles, percent and integer seed', async t => {
    const window = environment(t); const root = window.document.getElementById('first'); const driver = port(1);
    const widget = new window.TestWidget(root, driver); await widget.init();
    assert.equal(root.querySelectorAll('input[type="number"]').length, 0);
    assert.equal(root.querySelectorAll('input[role="spinbutton"]').length, 6);
    const amplitude = root.querySelector('[data-field="deviation"]');
    amplitude.focus();
    amplitude.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
    assert.equal(amplitude.value, '1.0');
    assert.equal(amplitude.selectionEnd - amplitude.selectionStart, 1);
    root.dispatchEvent(new window.WheelEvent('wheel', {deltaY: -40, bubbles: true, cancelable: true}));
    await settle();
    assert.deepEqual(driver.calls, [[0, 'deviation', 1.1]]);
    assert.equal(amplitude.value, '1.1');
    const phase = root.querySelector('[data-field="phase"]');
    phase.focus(); phase.value = '-1'; phase.dispatchEvent(new window.Event('input', {bubbles: true}));
    phase.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Enter', bubbles: true})); await settle();
    phase.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true})); await new Promise(resolve => setTimeout(resolve, 120));
    assert.equal(driver.values[0].phase, 0);
    const seed = root.querySelector('[data-field="seed"]');
    seed.disabled = false; seed.focus();
    seed.value = '1.5'; seed.dispatchEvent(new window.Event('input', {bubbles: true}));
    seed.dispatchEvent(new window.KeyboardEvent('keydown', {key: 'Enter', bubbles: true})); await settle();
    assert.equal(driver.values[0].seed, 1);
    assert.equal(seed.getAttribute('aria-invalid'), 'true');
    widget.dispose();
    const calls = driver.calls.length;
    root.dispatchEvent(new window.WheelEvent('wheel', {deltaY: -40, bubbles: true, cancelable: true}));
    await settle(); assert.equal(driver.calls.length, calls);
});
