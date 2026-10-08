const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../..');
function host(t, board, instrument) {
    const dom = new JSDOM(fs.readFileSync(path.join(root, `examples/${board}/${instrument}/web/index.html`), 'utf8'), {runScripts:'outside-only'});
    t.after(() => dom.window.close());
    const w = dom.window, d = w.document;
    w.eval(ts.transpileModule(fs.readFileSync(path.join(root, 'web/phase-noise/measurements.ts'), 'utf8'),
        {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText + '\nwindow.Readout=PnaMeasurementReadout;');
    return {d,readout:new w.Readout(d)};
}
for (const [board, instrument] of [['alpha250','phase-noise-analyzer'],['alpha250-4','phase-noise-analyzer'],['red-pitaya','phase-noise-analyzer'],['alpha250','dpll']]) {
    test(`${board}/${instrument} uses common RMS conversions and precise jitter-band feedback`, t => {
        const {d,readout} = host(t,board,instrument);
        readout.render({carrier_power:-3.456, phase_jitter:.01234, time_jitter:5.67e-12, freq_lo:1000.125, freq_hi:1000000.5});
        assert.equal(d.querySelector('.carrier-power-span').textContent, '-3.46 dBm');
        assert.equal(d.querySelector('.phase-jitter-span').textContent, '12.34 mrad rms');
        assert.equal(d.querySelector('.time-jitter-span').textContent, '5.67 ps rms');
        assert.equal(d.querySelector('.phase-jitter-span sub').textContent, 'rms');
        assert.equal(d.querySelector('.time-jitter-span sub').textContent, 'rms');
        const range = d.getElementById('jitter-range');
        assert.equal(range.textContent,'1.000125 kHz – 1.000001 MHz');
        assert.equal(range.title,'1000.125 – 1000000.5 Hz');
        // Missing readings never retain prior units, values or interval details.
        readout.render({carrier_power:NaN,phase_jitter:Infinity,time_jitter:-Infinity,freq_lo:NaN,freq_hi:100});
        for (const selector of ['.carrier-power-span','.phase-jitter-span','.time-jitter-span','#jitter-range'])
            assert.equal(d.querySelector(selector).textContent,'—');
        assert.equal(range.title,'');
        readout.render({carrier_power:-.0001,phase_jitter:-1e-7,time_jitter:-1e-16,freq_lo:0,freq_hi:1e9});
        assert.equal(d.querySelector('.carrier-power-span').textContent,'0.00 dBm');
        assert.equal(d.querySelector('.phase-jitter-span').textContent,'0.00 mrad rms');
        assert.equal(d.querySelector('.time-jitter-span').textContent,'0.00 ps rms');
        assert.equal(range.textContent,'0 Hz – 1 GHz');
        readout.clear();
        for (const selector of ['.carrier-power-span','.phase-jitter-span','.time-jitter-span','#jitter-range'])
            assert.equal(d.querySelector(selector).textContent,'—');
        assert.equal(range.hasAttribute('title'),false);
    });
}

for (const [name, file, format] of [
    ['single-stream', 'web/phase-noise/analyzer/phase-noise-analyzer.ts', 'ffffd'],
    ['cross-spectrum', 'examples/alpha250-4/phase-noise-analyzer/web/phase-noise-analyzer.ts', 'fdddd']
]) {
    test(`${name} measurements retain the server tuple precision and averaging argument`, async t => {
        const dom = new JSDOM('', {runScripts:'outside-only'}), w = dom.window;
        t.after(() => w.close());
        w.eval(ts.transpileModule(['web/koheron.ts', 'web/phase-noise/measurements.ts', file]
            .map(pathname => fs.readFileSync(path.join(root, pathname), 'utf8')).join('\n'),
            {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText
            + '\nwindow.Client=Client; window.Analyzer=PhaseNoiseAnalyzer;');
        const values = [.012345, 3.5e-12, 123.25, 1000000.5, -3.456];
        const wire = new w.DataView(new w.ArrayBuffer(36));
        let offset = 0;
        values.forEach((value, i) => {
            if (format[i] === 'f') { wire.setFloat32(offset, value); offset += 4; }
            else { wire.setFloat64(offset, value); offset += 8; }
        });
        const client = Object.create(w.Client.prototype);
        client.getDriver = () => ({id:9, getCmds:() => ({get_measurements:{id:3,args:[{type:'uint32_t'}]}})});
        client._readBaseAsync = async (kind, command) => {
            assert.equal(kind, 'static'); assert.equal(command.devid, 9);
            const request = new w.DataView(command.data.buffer);
            assert.equal(request.getUint16(6), 3); assert.equal(request.getUint32(8), 400);
            return wire;
        };
        const driver = new w.Analyzer(client), result = await driver.getMeasurements(400);
        assert.deepEqual(Object.values(result), values.map((value,i) => format[i] === 'f' ? Math.fround(value) : value));
        client._readBaseAsync = async () => { throw new Error('Disconnected'); };
        await assert.rejects(driver.getMeasurements(400), /Disconnected/);
    });
}
