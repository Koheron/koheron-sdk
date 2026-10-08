// Real ALPHA250-4 decoder and shared workspace, with simulated board RPCs.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('typescript');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../../..');
const project = 'examples/alpha250-4/fft';
const settle = () => new Promise(resolve => setTimeout(resolve, 25));
async function host(t, failure = false) {
    const dom = new JSDOM(fs.readFileSync(path.join(root, project, 'web/index.html'), 'utf8'), {runScripts: 'outside-only', pretendToBeVisual: true});
    const w = dom.window, d = w.document;
    const state = {writes: [], reads: [], errors: [], exits: 0, fs: [250e6, 200e6], channel: 0, window: 1, reference: 2, dac: [.1,.2,.3,.4]};
    w.console.error = (...args) => state.errors.push(args);
    const timers = new Map(), frames = new Map(); let id = 0, time = 0;
    w.setTimeout = fn => { timers.set(++id, fn); return id; };
    w.clearTimeout = key => timers.delete(key);
    w.requestAnimationFrame = fn => { frames.set(++id, fn); return id; };
    w.cancelAnimationFrame = key => frames.delete(key);
    const flush = async () => {
        await settle(); const queued = Array.from(frames.values()); frames.clear();
        for (const fn of queued) { fn(time += 17); } await settle();
    };
    const canvas = {setTransform() {}, clearRect() {}, fillRect() {}, strokeRect() {}, fillText() {}, scale() {},
        save() {}, restore() {}, translate() {}, rotate() {}, drawImage() {}, putImageData() {}, beginPath() {}, moveTo() {}, lineTo() {}, stroke() {},
        createImageData(width, height) { return {data: new Uint8ClampedArray(width * height * 4)}; }};
    w.HTMLCanvasElement.prototype.getContext = () => canvas;
    w.HTMLCanvasElement.prototype.toBlob = function(callback) { callback(new Blob(['png'], {type:'image/png'})); };
    w.Imports = class {
        constructor(document) {
            const assets = [`${project}/web/fft`, 'web/clock-generator', 'web/precision-channels', 'web/temperature-sensor', 'web/power-monitor',
                'web/fft', 'web/fft/controls', 'web/fft/plot', 'web/fft/export-file', 'web/plot-basics'];
            for (const link of document.querySelectorAll('link[rel="import"]')) {
                const file = assets.map(dir => path.join(root, dir, link.getAttribute('href'))).find(fs.existsSync);
                assert(file, link.getAttribute('href'));
                const fragment = new JSDOM(fs.readFileSync(file, 'utf8'));
                document.getElementById(link.dataset.parent).append(document.importNode(fragment.window.document.querySelector('template').content, true));
                fragment.window.close();
            }
        }
    };
    w.Command = (driver, name, ...args) => ({driver, name, args});
    w.Client = class {
        async init() { if (failure) { throw new Error('Board offline'); } }
        exit() { state.exits++; }
        getDriver(name) { return {id: name, getCmds: () => new Proxy({}, {get: (_, key) => key})}; }
        send(command) {
            state.writes.push(command);
            if (command.name === 'set_input_channel') state.channel = command.args[0];
            if (command.name === 'set_fft_window') state.window = command.args[0];
            if (command.name === 'set_reference_clock') state.reference = command.args[0];
            if (command.name === 'set_sampling_frequency') state.fs = command.args[0] === 0 ? [200e6,200e6] : [250e6,250e6];
            if (command.name === 'set_dac_value_volts') state.dac[command.args[0]] = command.args[1];
        }
        async readUint32(command) {
            state.reads.push(command);
            if (command.name === 'get_fft_size') return 4096;
            if (command.name === 'get_window_index') return state.window;
            if (command.name === 'get_reference_clock') return state.reference;
            throw new Error(command.name);
        }
        async readTuple(command, format) {
            state.reads.push(command); assert.equal(format, 'ddIdd');
            return [...state.fs, state.channel, .25, .375];
        }
        async readFloat32Array(command) {
            state.reads.push(command);
            switch (command.name) {
                case 'read_psd': {
                    const data = new w.Float32Array(2048).fill(1e-12); data[8 + command.args[0]] = 1e-6;
                    if (state.holdPSD) { return new Promise(resolve => { state.releasePSD = () => resolve(data); }); }
                    return data;
                }
                case 'get_dac_values': return new w.Float32Array(state.dac);
                case 'get_adc_values': return new w.Float32Array([.001,.002,.003,.004]);
                case 'get_temperatures': return new w.Float32Array([30,40,50]);
                case 'get_supplies_ui': return new w.Float32Array([.1,12,.02,3.3]);
                default: throw new Error(command.name);
            }
        }
    };
    let range = {from:0,to:125}, drawn;
    w.$ = () => ({on() {}, off() {}, trigger() { range = {from:0,to:w.workspace.fft.status.fs/2e6}; }});
    w.PlotBasics = class {
        constructor() {} enableSpectrumReduction() {} enableBatchedLines() {} setLinY() {} needsRedraw() { return false; }
        setRangeX(from,to) { range = {from,to}; } getRangeX() { return range; } setVisibleRangeX(from,to) { range = {from,to}; }
        redraw(data,count,peak,label,cb) { drawn = {data,count,peak,label}; cb(); }
    };
    const files = ['web/instrument/events.ts', 'web/instrument/poller.ts','web/power-monitor/readout.ts','web/temperature-sensor/readout.ts','web/inputs/digit-input.ts',
        'web/clock-generator/clock-generator.ts','web/clock-generator/clock-generator-app.ts',
        'web/precision-channels/precision-adc.ts','web/precision-channels/precision-dac.ts','web/precision-channels/precision-channels-app.ts',
        'web/temperature-sensor/temperature-sensor.ts','web/power-monitor/power-monitor.ts','web/board-controls/alpha-fft.ts',
        'web/fft/driver.ts','web/fft/controls/fft-app.ts','web/fft/plot/spectrum-history.ts','web/fft/plot/spectrum-views.ts',
        'web/fft/plot/references.ts', 'web/fft/plot/plot.ts','web/fft/export-file/export-file.ts','web/fft/workspace.ts',`${project}/web/fft.ts`,`${project}/web/app.ts`];
    w.eval(ts.transpileModule(files.map(file => fs.readFileSync(path.join(root,file),'utf8')).join('\n'), {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText + '\nwindow.workspace = app;');
    w.dispatchEvent(new w.Event('HTMLImportsLoaded')); await flush();
    t.after(() => { w.dispatchEvent(new w.Event('pagehide')); w.close(); });
    assert.equal(state.errors.length, failure ? 1 : 0);
    return {w,d,state,timers,frames,flush,app:w.workspace,get drawn(){return drawn;}};
}

async function select(h, channel) {
    const input = h.d.querySelector(`[data-command='setInputChannel'][value='${channel}']`);
    input.checked = true; input.dispatchEvent(new h.w.Event('change'));
    await h.app.fft.getControlParameters(); await h.app.plot.updatePlot(); await h.flush();
}

test('ALPHA250-4 mounts the shared workspace, four inputs and read-only precision controls', async t => {
    const h = await host(t);
    assert.equal(h.d.getElementById('board-label').textContent,'ALPHA250-4');
    assert.equal(h.d.querySelector('.fft-generator').hidden,true);
    assert.equal(h.d.querySelectorAll('.fft-input').length,4);
    assert.equal(h.d.getElementById('instrument-controls').disabled,false);
    assert.equal(h.d.getElementById('sampling-frequency').hidden,false);
    assert.equal(h.state.writes.length,0);
    assert.deepEqual(Array.from(h.d.querySelectorAll('.precision-dac-input'),n=>n.value),['100','200','300','400']);
    assert.deepEqual(Array.from(h.d.querySelectorAll('.precision-adc-span'),n=>n.textContent),['1.0000','2.0000','3.0000','4.0000']);
    assert.equal(h.app.plot.plot_data.length,2048);
    assert.equal(h.app.plot.plot_data[0][0],0);
    assert.equal(h.app.plot.plot_data[8][0],8*250/4096);
    assert.equal(h.app.fft.startPSDStream,undefined);
});

test('four inputs map to the correct FFT engine and selected-pair frequency grid', async t => {
    const h = await host(t);
    for (const channel of [1,2,3,0]) {
        await select(h,channel);
        assert.equal(h.state.writes.at(-1).args[0],channel&1);
        assert.equal(h.state.reads.filter(c=>c.name==='read_psd').at(-1).args[0],channel>>1);
        assert.equal(h.app.plot.frameStatus.channel,channel);
        assert.equal(h.app.plot.frameStatus.fs,h.state.fs[channel>>1]);
        assert.equal(h.app.plot.plot_data[1][0],h.state.fs[channel>>1]/4096/1e6);
        assert.equal(h.app.plot.convertValue(1e-12,'dBm'),10*Math.log10(1e-12*1.5*h.state.fs[channel>>1]/4096/1e-3));
        assert.equal(h.app.plot.convertValue(1e-12,'nv-rtHz'),Math.sqrt(50e-12)*1e9);
    }
    const writes = h.state.writes.length;
    for (const invalid of [-1,4,1.5,NaN]) h.app.fft.setInputChannel(invalid);
    assert.equal(h.state.writes.length,writes);
});

test('late channel replies are discarded and captured reference metadata stays with its original input', async t => {
    const h=await host(t),plot=h.app.plot;
    plot.captureReference(); const reference=plot.references.items[0].status;
    h.state.holdPSD=true; const pending=h.app.fft.readSpectrum(); await settle();
    h.app.fft.setInputChannel(3);
    assert.equal(await h.app.fft.readSpectrum(),undefined);
    h.state.releasePSD(); assert.equal(await pending,undefined);
    h.state.holdPSD=false;
    await h.app.fft.getControlParameters(); await plot.updatePlot(); await h.flush();
    assert.equal(plot.frameStatus.channel,3); assert.equal(plot.frameStatus.fs,200e6);
    assert.equal(reference.channel,0); assert.equal(reference.fs,250e6);
    assert.strictEqual(plot.references.items[0].status,reference);
});

test('window, reference and sampling controls preserve commands and selected frame metadata', async t => {
    const h=await host(t);
    for (const [selector,value] of [["[data-command='setFFTWindow']",'3'],["[data-command='setReferenceClock'][value='0']",'0'],["[data-command='setSamplingFrequency'][value='0']",'0']]) {
        const input=h.d.querySelector(selector); input.value=value; input.dispatchEvent(new h.w.Event('change'));
        assert.equal(h.app.fft.waitingForSpectrum,true);
        await h.app.fft.getControlParameters();
    }
    await h.app.plot.updatePlot(); await h.flush();
    assert.deepEqual(h.state.writes.map(c=>[c.name,...c.args]),[['set_fft_window',3],['set_reference_clock',0],['set_sampling_frequency',0]]);
    assert.equal(h.app.plot.frameStatus.window_index,3);
    assert.equal(h.app.plot.frameStatus.clkIndex,'0'); assert.equal(h.app.plot.frameStatus.fs,200e6);
});

test('pause, history, full-bin CSV/PNG exports and precision drafts use the shared design', async t => {
    const h=await host(t),plot=h.app.plot;
    const input=h.d.querySelector('.precision-dac-input'); input.value='123.456'; input.dispatchEvent(new h.w.Event('input'));
    h.app.board.precisionDacChanged(new h.w.Float32Array(h.state.dac)); assert.equal(input.value,'123.456');
    assert.equal(h.state.writes.length,0);
    input.dispatchEvent(new h.w.KeyboardEvent('keydown',{key:'Enter',bubbles:true}));
    for(const [id,callback] of Array.from(h.timers)) { h.timers.delete(id); callback(); } await h.flush();
    assert.deepEqual(h.state.writes.map(c=>[c.name,...c.args]),[['set_dac_value_volts',0,.123456]]);
    h.d.getElementById('pause-display').click(); const reads=h.state.reads.length;
    await plot.updatePlot(); assert.equal(h.state.reads.length,reads);
    h.d.getElementById('pause-display').click(); await h.flush();
    h.w.Blob=Blob; let output,filename;
    h.app.exportFile.download=async(blob,name)=>{output=await blob.text();filename=name;};
    h.app.exportFile.exportData(); await settle();
    assert(output.startsWith('Koheron ALPHA250-4 FFT')); assert(output.includes('Frequency (MHz)'));
    assert(output.includes(String(2047*250/4096)));
    plot.views.mode='spectrogram'; h.app.exportFile.exportData(); await settle(); assert(output.includes('Age (s) / Frequency (MHz)'));
    plot.views.mode='density'; h.app.exportFile.exportData(); await settle(); assert(filename.includes('density'));
    h.app.exportFile.exportPlot(); await settle(); assert.equal(filename,'koheron_fft.png');
});

test('failure and page exit disable acquisition and stop late rendering and hardware writes', async t => {
    const failed=await host(t,true);
    assert.equal(failed.d.getElementById('connection-error').hidden,false);
    assert.equal(failed.d.getElementById('instrument-controls').disabled,true); assert.equal(failed.state.exits,1);
    const h=await host(t); h.state.holdPSD=true; const pending=h.app.plot.updatePlot(); await settle();
    const original=h.drawn; h.w.dispatchEvent(new h.w.Event('pagehide')); h.state.releasePSD(); await pending; await h.flush();
    assert.strictEqual(h.drawn,original); assert.equal(h.state.exits,1);
    assert.equal(h.app.plot.running,false); assert.equal(h.app.fftApp.running,false);
    const writes=h.state.writes.length;
    for (const input of h.d.querySelectorAll('.clkgen-input, .fft-input, .fft-select')) input.dispatchEvent(new h.w.Event('change'));
    assert.equal(h.state.writes.length,writes); assert.equal(h.d.getElementById('instrument-controls').disabled,true);
});

test('control decoder preserves the existing binary tuple and selected-pair sample rate', async () => {
    const vm = require('node:vm');
    const files = ['web/koheron.ts','web/fft/driver.ts','web/clock-generator/clock-generator.ts',
        'web/precision-channels/precision-adc.ts','web/precision-channels/precision-dac.ts',
        'web/temperature-sensor/temperature-sensor.ts','web/power-monitor/power-monitor.ts',`${project}/web/fft.ts`];
    const context = vm.createContext({console,assert});
    vm.runInContext(ts.transpileModule(files.map(file=>fs.readFileSync(path.join(root,file),'utf8')).join('\n'),
        {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText,context);
    await vm.runInContext(`(async () => {
        const wire = new DataView(new ArrayBuffer(36));
        wire.setFloat64(0,250e6); wire.setFloat64(8,200e6); wire.setUint32(16,1);
        wire.setFloat64(20,.25); wire.setFloat64(28,.375);
        Command = (id, command, ...args) => ({id, command, args});
        const client = {
            getDriver: () => ({id:1,getCmds:()=>({get_control_parameters:1,get_window_index:2,get_reference_clock:3,set_input_channel:4})}),
            readTuple: async (_,fmt) => Client.prototype.deserialize(fmt,wire),
            readUint32: async command => command.command === 2 ? 3 : 0,
            send(command) { wire.setUint32(16,command.args[0]); }
        };
        const fft = new FFT(client);
        const initial = await fft.getControlParameters();
        assert.equal(initial.fs,250e6); assert.equal(initial.channel,1);
        assert.equal(initial.W1,.25); assert.equal(initial.W2,.375);
        assert.equal(initial.window_index,3); assert.equal(initial.clkIndex,'0');
        fft.setInputChannel(2);
        const selected = await fft.getControlParameters();
        assert.equal(selected.fs,200e6); assert.equal(selected.channel,2);
    })()`, context);
});
