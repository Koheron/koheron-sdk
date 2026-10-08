// Real Red Pitaya page, frequency editor and DDS RPC adapter; no board required.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');
const root = path.resolve(__dirname, '../../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const settle = () => new Promise(resolve => setTimeout(resolve, 25));
async function host(t, failure = false) {
    const dom = new JSDOM(read('examples/red-pitaya/dual-dds/web/index.html'), {runScripts:'outside-only', pretendToBeVisual:true});
    const w = dom.window, d = w.document, state = {values:[10e6,20e6], writes:[], reads:0, exits:0};
    const timers = new Map(); let id = 1000000;
    const timeout = w.setTimeout.bind(w);
    w.setTimeout = (callback, delay) => {
        if (delay === 1000) {timers.set(++id,callback); return id;}
        return timeout(callback,delay);
    };
    const clear = w.clearTimeout.bind(w);
    w.clearTimeout = key => {timers.delete(key); clear(key);};
    w.Command = (id,name,...args) => ({id,name,args});
    w.Imports = class {
        constructor() {
            const template = new w.DOMParser().parseFromString(read('web/dds-frequency/dds-frequency.html'),'text/html').querySelector('template');
            d.getElementById('dds-frequency').append(d.importNode(template.content,true));
        }
    };
    w.Client = class {
        async init() {if(failure) throw new Error('Offline');}
        exit() {state.exits++;}
        getDriver(name) {assert.equal(name,'DualDDS'); return {id:7,getCmds:()=>({set_dds_freq:'write',get_control_parameters:'read'})};}
        send(command) {state.writes.push(command); state.values[command.args[0]]=command.args[1];}
        async readTuple(command,format) {
            assert.equal(command.id,7); assert.equal(command.name,'read'); assert.equal(format,'dd');
            state.reads++; return state.values.slice();
        }
    };
    const files = ['web/inputs/digit-input.ts','web/instrument/poller.ts','web/dds-frequency/dds-frequency.ts',
        'examples/red-pitaya/dual-dds/web/dual_dds.ts','examples/red-pitaya/dual-dds/web/app.ts'];
    w.eval(ts.transpileModule(files.map(read).join('\n'), {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText+'\nwindow.app=app;');
    w.dispatchEvent(new w.Event('HTMLImportsLoaded')); await settle();
    t.after(()=>{w.dispatchEvent(new w.Event('pagehide')); w.close();});
    const type = (input,value) => {input.value=value;input.dispatchEvent(new w.Event('input'));};
    const enter = input => input.dispatchEvent(new w.KeyboardEvent('keydown',{key:'Enter',bubbles:true}));
    return {w,d,state,timers,type,enter};
}

test('DDS opens with shared digit editors, no sliders and read-only startup', async t => {
    const h = await host(t);
    assert.equal(h.d.querySelectorAll('input[type="range"], input[type="number"]').length,0);
    assert.equal(h.d.querySelectorAll('input[role="spinbutton"]').length,2);
    assert.equal(h.d.getElementById('dds-frequency-controls').disabled,false);
    assert.equal(Number(h.d.querySelector('[data-channel="0"]').value.replace(/\s/g,'')),10);
    assert.equal(h.state.writes.length,0); assert(h.state.reads>0);
    assert.equal(h.d.querySelector('[data-channel="0"]').getAttribute('aria-valuemax'),'125000000');
});

test('DDS commits typed frequencies in Hz, validates limits and preserves drafts during polling', async t => {
    const h = await host(t), input=h.d.getElementById('dds-frequency-1');
    h.type(input,'12.345678'); assert.equal(h.state.writes.length,0);
    h.enter(input); await settle();
    assert.equal(h.state.writes.length,1);
    assert.deepEqual(h.state.writes[0],{id:7,name:'write',args:[1,12345678]});
    assert.equal(Number(h.d.getElementById('dds-frequency-0').value.replace(/\s/g,'')),10);
    for(const value of ['','-1','125.000001','NaN','Infinity']) {
        h.type(input,value);h.enter(input);await settle();
        assert.equal(input.getAttribute('aria-invalid'),'true');
    }
    assert.equal(h.state.writes.length,1);
    h.type(input,'34'); h.state.values[1]=99e6;
    const callback=h.timers.values().next().value; h.timers.clear(); callback(); await settle();
    assert.equal(input.value,'34');
    input.dispatchEvent(new h.w.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));
    assert.equal(Number(input.value.replace(/\s/g,'')),99);
    for (const value of ['125','0']) {h.type(input,value);h.enter(input);await settle();}
    assert.equal(h.state.values[1],0);
    assert.deepEqual(h.state.writes.slice(-2).map(command=>command.args),[[1,125e6],[1,0]]);
});

test('DDS digit tuning and exit preserve channel isolation and stop commands/readbacks', async t => {
    const h=await host(t),input=h.d.getElementById('dds-frequency-0');
    input.focus(); input.dispatchEvent(new h.w.KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true})); await settle();
    assert.equal(h.state.writes.length,1); assert.equal(h.state.writes[0].args[0],0);
    assert.equal(h.state.values[1],20e6);
    input.dispatchEvent(new h.w.KeyboardEvent('keydown',{key:'ArrowLeft',bubbles:true}));
    input.dispatchEvent(new h.w.KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true}));
    h.w.dispatchEvent(new h.w.Event('pagehide'));
    const reads=h.state.reads,writes=h.state.writes.length;
    await new Promise(resolve=>setTimeout(resolve,120));
    h.type(input,'42');h.enter(input);await settle();
    assert.equal(h.state.writes.length,writes); assert.equal(h.state.reads,reads);
    assert.equal(h.timers.size,0);assert.equal(h.state.exits,1);
    assert.equal(h.d.getElementById('dds-frequency-controls').disabled,true);
});

test('DDS connection failure leaves controls disabled without writes', async t => {
    const h=await host(t,true);
    assert.equal(h.d.getElementById('dds-frequency-status').textContent,'DDS unavailable');
    assert.equal(h.d.getElementById('dds-frequency-controls').disabled,true);
    assert.equal(h.state.writes.length,0);assert.equal(h.state.exits,1);
});
