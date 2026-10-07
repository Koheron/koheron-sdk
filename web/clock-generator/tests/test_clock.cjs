const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('typescript');

function fixture() {
    const commands = [];
    const client = {
        getDriver(name) { assert.equal(name, 'ClockGenerator'); return {id: 7, getCmds: () => ({get_reference_clock: 3, get_dac_sampling_freq: 4, set_reference_clock: 5, set_sampling_frequency: 6})}; },
        readUint32(command, callback) {
            commands.push(command);
            if (callback) { callback(2); return; }
            return Promise.resolve(2);
        },
        readFloat64(command) { commands.push(command); return Promise.resolve(250e6); },
        send(command) { commands.push(command); }
    };
    const context = vm.createContext({Command: (id, command, ...args) => ({id, command, args})});
    const source = fs.readFileSync(path.join(__dirname, '../clock-generator.ts'), 'utf8');
    vm.runInContext(ts.transpileModule(source, {compilerOptions: {target: ts.ScriptTarget.ES5}}).outputText
        + '\nglobalThis.ClockGenerator = ClockGenerator;', context);
    return {clock: new context.ClockGenerator(client), client, commands};
}

test('clock adapter construction performs no reads or writes', () => {
    assert.deepEqual(fixture().commands, []);
});

test('reference-clock read preserves both callback and promise contracts', async () => {
    const {clock, commands} = fixture();
    let reference;
    assert.equal(clock.getReferenceClock(value => { reference = value; }), undefined);
    assert.equal(reference, 2);
    assert.equal(await clock.getReferenceClock(), 2);
    assert.deepEqual(commands, [{id: 7, command: 3, args: []}, {id: 7, command: 3, args: []}]);
});

test('DAC sample-rate reads and clock writes retain their RPC IDs and argument units', async () => {
    const {clock, commands} = fixture();
    assert.equal(await clock.getDacSamplingFrequency(), 250e6);
    clock.setReferenceClock(0);
    clock.setSamplingFrequency(1);
    assert.deepEqual(commands, [{id: 7, command: 4, args: []}, {id: 7, command: 5, args: [0]}, {id: 7, command: 6, args: [1]}]);
});

test('clock read failures propagate to the connection owner', async () => {
    const {clock, client} = fixture();
    client.readUint32 = client.readFloat64 = () => Promise.reject(new Error('Disconnected'));
    await assert.rejects(clock.getReferenceClock(), /Disconnected/);
    await assert.rejects(clock.getDacSamplingFrequency(), /Disconnected/);
});
