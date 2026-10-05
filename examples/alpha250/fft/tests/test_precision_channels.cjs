const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const settle = () => new Promise(resolve => setTimeout(resolve, 25));

async function host(t) {
    const markup = fs.readFileSync(path.join(root, 'examples/alpha250/fft/web/precision-channels/precision-channels.html'), 'utf8');
    const dom = new JSDOM(markup, {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const window = dom.window;
    window.document.body.append(window.document.querySelector('template').content.cloneNode(true));
    const values = Float32Array.from([.1, .2, .3, .4]);
    const writes = [];
    const driver = {
        setDac(channel, volts) { writes.push([channel, volts]); values[channel] = volts; },
        async getDacValues() { return values.slice(); }
    };
    for (const [file, exports] of [
        ['web/phase-modulator/frequency-input.ts', ['NumberInput']],
        ['examples/alpha250/fft/web/precision-channels/precision-channels-app.ts', ['PrecisionChannelsApp']]
    ]) {
        window.eval(ts.transpileModule(fs.readFileSync(path.join(root, file), 'utf8'), {
            compilerOptions: {target: ts.ScriptTarget.ES2020}
        }).outputText + exports.map(name => `\nwindow.${name} = ${name};`).join(''));
    }
    const app = new window.PrecisionChannelsApp(window.document, driver);
    t.after(() => app.dispose());
    await app.init();
    const inputs = Array.from(window.document.querySelectorAll('.precision-dac-input'));
    const type = (input, value) => {
        input.value = value;
        input.dispatchEvent(new window.Event('input', {bubbles: true}));
    };
    const key = (input, key) => input.dispatchEvent(new window.KeyboardEvent('keydown', {key, bubbles: true}));
    return {window, values, writes, app, inputs, type, key};
}

test('precision DAC startup reads four settings without writes and has no sliders', async t => {
    const h = await host(t);
    assert.equal(h.window.document.querySelectorAll('input[type="range"]').length, 0);
    assert.deepEqual(h.inputs.map(input => input.getAttribute('role')), Array(4).fill('spinbutton'));
    assert.deepEqual(h.inputs.map(input => input.value), ['100', '200', '300', '400']);
    assert.equal(h.writes.length, 0);
    h.type(h.inputs[2], '12.345 mV');
    assert.equal(h.writes.length, 0);
    h.key(h.inputs[2], 'Enter'); await settle();
    assert.deepEqual(h.writes, [[2, .012345]]);
    assert.equal(h.inputs[2].value, '12.345');
    assert.equal(h.inputs[1].value, '200');
    assert.equal(h.inputs[2].getAttribute('aria-valuemax'), '2500');
});

test('precision DAC telemetry preserves drafts and invalid entries until correction or Escape', async t => {
    const h = await host(t);
    const input = h.inputs[0];
    h.type(input, '12.345');
    h.app.setValues(h.values);
    assert.equal(input.value, '12.345');
    for (const invalid of ['', '-1', '2501', 'NaN', 'Infinity']) {
        h.type(input, invalid); h.key(input, 'Enter'); await settle();
        input.dispatchEvent(new h.window.Event('blur'));
        h.app.setValues(h.values);
        assert.equal(input.value, invalid);
        assert.equal(input.getAttribute('aria-invalid'), 'true');
    }
    assert.equal(h.writes.length, 0);
    h.key(input, 'Escape');
    assert.equal(input.value, '100');
    assert.equal(input.hasAttribute('aria-invalid'), false);
    h.type(input, '2500'); h.key(input, 'Enter'); await settle();
    assert.deepEqual(h.writes, [[0, 2.5]]);
});

test('precision DAC digit tuning uses selected place value and stops on disposal', async t => {
    const h = await host(t);
    const input = h.inputs[1];
    input.focus();
    h.key(input, 'ArrowUp'); await settle();
    assert.deepEqual(h.writes, [[1, .201]]);
    assert.equal(input.value, '201');
    h.key(input, 'ArrowLeft');
    h.key(input, 'ArrowUp');
    h.app.dispose();
    await new Promise(resolve => setTimeout(resolve, 120));
    assert.equal(h.writes.length, 1);
    h.app.setValues(Float32Array.from([0, 0, 0, 0]));
    assert.notEqual(input.value, '0');
});
