// Run with NODE_PATH pointing to the SDK node_modules in an isolated worktree.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const source = path.resolve(__dirname, '../../../../web/dds-frequency/dds-frequency.ts');
const context = vm.createContext({assert, console});
vm.runInContext(ts.transpileModule(fs.readFileSync(source, 'utf8'), {
    compilerOptions: {target: ts.ScriptTarget.ES2020}
}).outputText, context);
vm.runInContext(`
    const calls = [];
    function makeInput(type, channel) {
        return {
            type, value: '40', max: '125', dataset: {command: 'setDDSFreq', channel},
            events: {}, addEventListener(name, fn) { this.events[name] = fn; },
            get valueAsNumber() { return this.value === '' ? NaN : Number(this.value); },
            // Browser number inputs mark values outside their native constraints invalid.
            checkValidity() { return this.valueAsNumber >= 0 && this.valueAsNumber <= Number(this.max); },
            dispatch(name) { if (this.events[name]) this.events[name]({currentTarget: this}); }
        };
    }
    const number = makeInput('number', '0');
    const range = makeInput('range', '0');
    const otherChannel = makeInput('number', '1');
    const inputs = [number, range, otherChannel];
    const doc = {
        getElementsByClassName() { return inputs; },
        querySelector(selector) { return inputs.find(input =>
            selector.includes("type='" + input.type + "'") &&
            selector.includes("data-channel='" + input.dataset.channel + "'")); }
    };
    new DDSFrequency(doc, {setDDSFreq(channel, frequency) { calls.push([channel, frequency]); }});
    number.value = '';
    number.dispatch('input');
    number.dispatch('change');
    for (const invalid of ['-1', '126', 'NaN', 'Infinity']) {
        number.value = invalid;
        number.dispatch('change');
    }
    assert.equal(calls.length, 0);
    assert.equal(range.value, '40');
    number.value = '40.008545';
    number.dispatch('input');
    assert.equal(calls.length, 0); // Typing is not a hardware command.
    number.dispatch('change');
    assert.deepEqual(calls, [['0', 40008545]]);
    assert.equal(range.value, '40.008545');
    assert.equal(otherChannel.value, '40');
    range.value = '50';
    range.dispatch('input');
    range.dispatch('change');
    assert.equal(calls.length, 2); // Slider release does not duplicate its live command.
    assert.deepEqual(calls[1], ['0', 50000000]);
    assert.equal(number.value, '50');
    number.max = '100'; // A new sample rate updates the limit during editing.
    number.value = '110';
    number.dispatch('change');
    assert.equal(calls.length, 2);
    otherChannel.value = '25'; // A missing paired slider is also supported.
    otherChannel.dispatch('change');
    assert.deepEqual(calls[2], ['1', 25000000]);
`, context);
const precisionSource = path.resolve(__dirname, '../web/precision-channels/precision-channels-app.ts');
vm.runInContext(ts.transpileModule(fs.readFileSync(precisionSource, 'utf8'), {
    compilerOptions: {target: ts.ScriptTarget.ES2020}
}).outputText, context);
vm.runInContext(`
{
    const writes = [];
    const number = makeInput('number', '2');
    const range = makeInput('range', '2');
    number.dataset.command = range.dataset.command = 'setDac';
    number.max = range.max = '2500';
    const doc = {
        getElementsByClassName() { return [number, range]; },
        querySelector(selector) { return selector.includes("type='range'") ? range : number; }
    };
    new PrecisionChannelsApp(doc, {setDac(channel, volts) { writes.push([channel, volts]); }});
    for (const value of ['', '-1', '2501', 'NaN', 'Infinity']) {
        number.value = value;
        number.dispatch('change');
    }
    assert.equal(writes.length, 0);
    assert.equal(range.value, '40');
    number.value = '12.345';
    number.dispatch('input');
    assert.equal(writes.length, 0);
    number.dispatch('change');
    assert.deepEqual(writes[0], ['2', .012345]);
    assert.equal(range.value, '12.345');
    range.value = '2500';
    range.dispatch('input');
    range.dispatch('change');
    assert.equal(writes.length, 2);
    assert.deepEqual(writes[1], ['2', 2.5]);
    assert.equal(number.value, '2500');
}
`, context);
console.log('DDS and precision DAC commits, validation, pairing and live sliders: PASS');
