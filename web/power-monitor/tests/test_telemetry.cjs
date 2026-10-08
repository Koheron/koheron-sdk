const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('typescript');

for (const [folder, name, method, rpc] of [
    ['temperature-sensor', 'TemperatureSensor', 'getTemperatures', 'get_temperatures'],
    ['power-monitor', 'PowerMonitor', 'getSuppliesUI', 'get_supplies_ui']
]) {
    test(`${name} preserves callback and Promise payloads and read failures`, async () => {
        const calls = [], payload = new Float32Array([1, 2, 3]);
        const client = {
            getDriver(driver) {
                assert.equal(driver, name);
                return {id: 7, getCmds: () => ({[rpc]: 4})};
            },
            readFloat32Array(command, callback) {
                calls.push(command);
                if (callback) { callback(payload); return; }
                return Promise.resolve(payload);
            }
        };
        const context = vm.createContext({Command: (id, command) => ({id, command})});
        const source = fs.readFileSync(path.join(__dirname, '../../', folder, `${folder}.ts`), 'utf8');
        vm.runInContext(ts.transpileModule(source, {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText
            + `\nglobalThis.Adapter = ${name};`, context);
        const adapter = new context.Adapter(client);
        assert.deepEqual(calls, []);
        let value;
        assert.equal(adapter[method](result => {value = result;}), undefined);
        assert.equal(value, payload);
        assert.equal(await adapter[method](), payload);
        assert.deepEqual(calls, [{id: 7, command: 4}, {id: 7, command: 4}]);
        client.readFloat32Array = () => Promise.reject(new Error('Disconnected'));
        await assert.rejects(adapter[method](), /Disconnected/);
    });
}

test('shared templates map telemetry to supply units and consistent temperature precision', () => {
    const {JSDOM} = require('jsdom');
    const dom = new JSDOM('<main></main>', {runScripts: 'outside-only'});
    try {
        const {window} = dom;
        for (const folder of ['power-monitor', 'temperature-sensor']) {
            const directory = path.join(__dirname, '../../', folder);
            const fragment = new JSDOM(fs.readFileSync(path.join(directory, `${folder}.html`), 'utf8'));
            window.document.querySelector('main').append(window.document.importNode(
                fragment.window.document.querySelector('template').content, true));
            fragment.window.close();
            window.eval(ts.transpileModule(fs.readFileSync(path.join(directory, 'readout.ts'), 'utf8'), {
                compilerOptions: {target: ts.ScriptTarget.ES2020}
            }).outputText);
        }
        const supplies = window.document.querySelectorAll('.supply-span');
        const temperatures = window.document.querySelectorAll('.temperature-span');
        window.updateSupplyReadouts(supplies, [.1, 12, .02, 3.3]);
        assert.deepEqual(Array.from(supplies, span => span.textContent), ['12.000', '3.300', '100.0', '20.0']);
        window.updateTemperatureReadouts(temperatures, [30.1234, 40.5678, 50.9876]);
        assert.deepEqual(Array.from(temperatures, span => span.textContent), ['30.1', '40.6', '51.0']);
        const observer = new window.MutationObserver(() => {});
        observer.observe(window.document.querySelector('main'), {subtree: true, childList: true});
        window.updateSupplyReadouts(supplies, [.1, 12, .02, 3.3]);
        window.updateTemperatureReadouts(temperatures, [30.1234, 40.5678, 50.9876]);
        assert.equal(observer.takeRecords().length, 0);
        observer.disconnect();
    } finally {
        dom.window.close();
    }
});
