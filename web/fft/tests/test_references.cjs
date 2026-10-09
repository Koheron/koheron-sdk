const assert = require('node:assert/strict');
const fs = require('node:fs');
const test = require('node:test');
const ts = require('../../transpile.cjs');
const {JSDOM} = require('jsdom');
const source = ts.transpileModule(fs.readFileSync('web/fft/plot/references.ts', 'utf8'),
    {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText;
const status = () => ({fs:250e6, W1:.25, W2:.375, channel:0, window_index:1, clkIndex:'2', dds_freq:[1e6, 0]});
function host(t, board = 'alpha250-4') {
    const dom = new JSDOM('<body></body>', {runScripts:'outside-only'});
    const w = dom.window;
    w.eval(source + '\nwindow.FFTReferences = FFTReferences; window.FFTReferencePanel = FFTReferencePanel;');
    t.after(() => w.close());
    const refs = new w.FFTReferences(board);
    return {w, refs};
}
test('named captures own their samples and metadata and round trip without rendering caches', t => {
    const {w, refs} = host(t), s = status(), psd = new Float32Array([1e-12, 2e-12, NaN, 0]);
    refs.capture(psd, s, 8); refs.items[0].name = 'Before "filter", input';
    refs.items[0].visible = false; refs.items[0].data = [[999, 999]]; refs.items[0].unit = 'dBm';
    psd[0] = 10; s.dds_freq[0] = 2e6;
    refs.capture(psd, s, 8);
    assert.equal(refs.items[0].status.dds_freq[0], 1e6);
    assert.notEqual(refs.items[0].psd[0], psd[0]);
    assert.notEqual(refs.items[0].color, refs.items[1].color);
    const text = refs.serialize(); assert(!('data' in JSON.parse(text).references[0])); assert(!('unit' in JSON.parse(text).references[0]));
    const loaded = new w.FFTReferences('alpha250-4'); loaded.load(text);
    assert.deepEqual(JSON.parse(loaded.serialize()), JSON.parse(text));
    assert(Number.isNaN(loaded.items[0].psd[2]));
    assert.equal(loaded.items[0].visible, false);
    assert.equal(loaded.items[0].data, undefined);
    loaded.capture(psd, s, 8); assert.equal(loaded.items.length, 3);
});
test('malformed, incompatible and over-capacity files leave captures intact', t => {
    const {refs} = host(t); refs.capture(new Float32Array(4).fill(1e-12), status(), 8);
    const original = refs.serialize(), file = JSON.parse(original);
    for (const mutate of [
        f => {f.version = 2;}, f => {f.board = 'alpha15';},
        f => {f.references[0].psd.pop();}, f => {f.references[0].psd[0] = -1;},
        f => {f.references[0].psd[0] = 'NaN';}, f => {f.references[0].status.W1 = 0;},
        f => {f.references[0].color = 'url(https://example.com)';},
        f => {f.references.push({...f.references[0], name:''});}
    ]) {
        const invalid = JSON.parse(original); mutate(invalid);
        assert.throws(() => refs.load(JSON.stringify(invalid)));
        assert.equal(refs.serialize(), original);
    }
    assert.throws(() => refs.load('{'));
    for (let i=1;i<8;i++) refs.load(JSON.stringify(file));
    assert.throws(() => refs.load(JSON.stringify(file)), /exceed 8/);
    assert.throws(() => refs.capture(new Float32Array(4), status(), 8), /up to 8/);
    assert.equal(refs.items.length, 8);
});
test('ALPHA15 nonuniform voltage references retain grids, bandwidths and ranges', t => {
    const {w, refs} = host(t, 'alpha15'), s = status();
    s.spectrum = {frequencies:[0, 3.5, 1000, 1e6], bandwidths:[5,5,50,500], binSpacings:[3.5, 50, 500], unit:'Hz', logarithmic:true};
    s.inputRanges = [2.048, 8.192];
    refs.capture(new Float32Array(4).fill(1e-16), s, 8192);
    s.spectrum.frequencies[1] = 999; s.inputRanges[0] = 8.192;
    const loaded = new w.FFTReferences('alpha15'); loaded.load(refs.serialize());
    assert.equal(loaded.items[0].status.spectrum.frequencies[1], 3.5);
    assert.equal(loaded.items[0].status.spectrum.bandwidths[3], 500);
    assert.equal(loaded.items[0].status.inputRanges[0], 2.048);
    const bad = JSON.parse(refs.serialize()); bad.references[0].status.spectrum.bandwidths.pop();
    assert.throws(() => loaded.load(JSON.stringify(bad)));
});
test('panel names are text; visibility, removal, saved files and async load preserve the collection', async t => {
    const {w, refs} = host(t), d = w.document;
    const template = d.createElement('div'); template.innerHTML = fs.readFileSync('web/fft/workspace.html','utf8');
    d.body.append(template.querySelector('template').content.cloneNode(true));
    let redraws = 0, saved;
    w.URL.createObjectURL = blob => {saved = blob; return 'blob:references';};
    w.URL.revokeObjectURL = () => {};
    w.HTMLAnchorElement.prototype.click = () => {};
    const panel = new w.FFTReferencePanel(d, refs, () => redraws++, item => refs.replace(item, new Float32Array(4).fill(3e-12), status(), 8));
    refs.capture(new Float32Array(4).fill(1e-12), status(), 8);
    refs.capture(new Float32Array(4).fill(2e-12), status(), 8);
    const name = d.querySelector('.reference-row input[type=text]');
    name.value = '<img src=x onerror=alert(1)>'; name.dispatchEvent(new w.Event('change'));
    assert.equal(refs.items[0].name, name.value); assert.equal(d.querySelectorAll('#reference-list img').length, 0);
    const toggle = d.querySelector('.reference-row input[type=checkbox]'); toggle.click();
    assert.equal(refs.items[0].visible, false);
    d.getElementById('save-references').click(); assert(saved);
    const text = await new Promise(resolve => {const reader = new w.FileReader(); reader.onload = () => resolve(reader.result); reader.readAsText(saved);});
    assert.equal(JSON.parse(text).references.length, 2);
    d.querySelector('.reference-row button[aria-label^=Remove]').click(); assert.equal(refs.items.length, 1);
    const file = d.getElementById('reference-file');
    Object.defineProperty(file, 'files', {configurable:true, value:[{name:'saved.json',size:text.length, text:async () => text}]});
    file.dispatchEvent(new w.Event('change')); await new Promise(resolve => setImmediate(resolve));
    assert.equal(refs.items.length, 3, d.getElementById('reference-message').textContent); assert.match(d.getElementById('reference-message').textContent, /Added 2 references from saved/);
    assert.equal(d.querySelectorAll('#reference-list img').length, 0);
    const count = refs.items.length;
    Object.defineProperty(file, 'files', {value:[{name:'bad.json',size:1,text:async () => '{'}]});
    file.dispatchEvent(new w.Event('change')); await new Promise(resolve => setImmediate(resolve));
    assert.equal(refs.items.length, count); assert(d.getElementById('reference-message').textContent);
    let finish;
    Object.defineProperty(file, 'files', {value:[{name:'late.json',size:text.length,text:() => new Promise(resolve => {finish = resolve;})}]});
    file.dispatchEvent(new w.Event('change')); panel.dispose(); finish(text); await new Promise(resolve => setImmediate(resolve));
    assert.equal(refs.items.length, count); assert(redraws > 3);
});

test('recapture preserves identity and can be undone; removal and clearing are recoverable', t => {
    const {refs} = host(t);
    refs.capture(new Float32Array(4).fill(1e-12), status(), 8);
    const original = refs.items[0]; original.name = 'Baseline'; original.visible = false;
    refs.replace(original, new Float32Array(4).fill(2e-12), {...status(), channel:1}, 8);
    assert.equal(refs.items.length, 1); assert.equal(refs.items[0].name, 'Baseline');
    assert.equal(refs.items[0].color, original.color); assert.equal(refs.items[0].visible, false);
    assert.equal(refs.items[0].status.channel, 1); assert.notEqual(refs.items[0].psd[0], original.psd[0]);
    refs.items[0].name = 'Renamed baseline'; refs.items[0].visible = true;
    refs.undo(); assert.strictEqual(refs.items[0], original); assert.equal(refs.undoLabel, '');
    assert.equal(original.name, 'Renamed baseline'); assert.equal(original.visible, true);
    assert.equal(original.status.channel, 0);
    refs.capture(new Float32Array(4).fill(3e-12), status(), 8);
    const second = refs.items[1]; refs.remove(original); assert.strictEqual(refs.items[0], second);
    refs.undo(); assert.strictEqual(refs.items[0], original); assert.strictEqual(refs.items[1], second);
    refs.clear(); assert.equal(refs.items.length, 0); assert(refs.undoLabel);
    assert.throws(() => refs.load('{')); assert(refs.undoLabel);
    refs.undo(); assert.equal(refs.items.length, 2);
    refs.remove(second); refs.capture(new Float32Array(4), status(), 8);
    assert.equal(refs.undoLabel, ''); refs.undo(); assert.equal(refs.items.length, 2);
});

test('reference names commit with Enter and cancel with Escape without changing samples', t => {
    const {w, refs} = host(t), d = w.document;
    const template = d.createElement('div'); template.innerHTML = fs.readFileSync('web/fft/workspace.html','utf8');
    d.body.append(template.querySelector('template').content.cloneNode(true));
    const panel = new w.FFTReferencePanel(d, refs, () => {}, () => {});
    refs.capture(new Float32Array(4).fill(1e-12), status(), 8);
    const item = refs.items[0], data = item.psd, input = d.querySelector('.reference-row input[type=text]');
    input.value = 'Before filter'; input.dispatchEvent(new w.KeyboardEvent('keydown', {key:'Enter'}));
    assert.equal(item.name, 'Before filter');
    input.value = 'Discard this edit'; input.dispatchEvent(new w.KeyboardEvent('keydown', {key:'Escape'}));
    assert.equal(item.name, 'Before filter'); assert.equal(input.value, 'Before filter'); assert.strictEqual(item.psd, data);
    d.querySelector('.reference-row button[aria-label^=Remove]').click();
    assert.equal(refs.items.length, 0); assert.equal(d.getElementById('reference-undo').hidden, false);
    d.getElementById('undo-reference').click(); assert.strictEqual(refs.items[0], item);
    assert.equal(d.getElementById('reference-count').textContent, '1 / 8');
    panel.dispose();
});
