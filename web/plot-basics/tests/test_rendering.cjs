const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('../../transpile.cjs');
const context = vm.createContext({});
vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../plot-basics.ts'), 'utf8'),
    {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nglobalThis.PlotBasics = PlotBasics;', context);
const PlotBasics = context.PlotBasics;

// Segment and pixel equivalence now run through the owned Flot renderer in
// real Chrome: npm test --prefix web/plotting.
const originalContext = vm.createContext({});
vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../../plotting/benchmark/baseline/plot-basics.ts'), 'utf8'),
    {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nglobalThis.PlotBasics = PlotBasics;', originalContext);
const OriginalPlotBasics = originalContext.PlotBasics;

test('linear and logarithmic spectrum reduction matches the original at every zoom boundary', () => {
    const data = Array.from({length: 16385}, (_, i) => [i + 1, -130 + Math.sin(i) * 15]);
    data[2048][1] = -70; data[2049][1] = -170; data[8192][1] = NaN;
    const original = data.map(row => row.slice());
    for (const logarithmic of [false, true]) for (const width of [1, 80, 800, 20000]) {
        for (const [from, to] of [[1,16385], [100,16000], [2040.5,2060.5], [0.5,1.5], [20000,21000], [.1,.5]]) {
            const expected = OriginalPlotBasics.reduceSpectrum(data, from, to, width, logarithmic);
            const actual = PlotBasics.reduceSpectrum(data, from, to, width, logarithmic);
            assert.deepEqual(Array.from(actual), Array.from(expected));
        }
    }
    assert.deepEqual(data, original, 'full-resolution source rows remain intact');
});

test('per-trace reduction buffers retain independent extrema and discard stale tails', () => {
    const data = Array.from({length: 4096}, (_, i) => [i + 1, -130 + Math.sin(i) * 15]);
    const reference = data.map(([x,y]) => [x,y+10]);
    const liveBuffer = [], referenceBuffer = [];
    const live = PlotBasics.reduceSpectrum(data, 1, 4096, 80, false, liveBuffer);
    const saved = live.slice();
    const ref = PlotBasics.reduceSpectrum(reference, 1, 4096, 80, false, referenceBuffer);
    assert.equal(live, liveBuffer); assert.equal(ref, referenceBuffer);
    assert.deepEqual(live, saved, 'reference reduction cannot replace live samples');
    const zoomed = PlotBasics.reduceSpectrum(data, 2000.5, 2010.5, 80, false, liveBuffer);
    assert.equal(zoomed, liveBuffer);
    assert.deepEqual(zoomed, data.slice(1999,2011), 'shorter redraws remove old buffer entries');
});

test('cached columns match uncached reduction after in-place grids, peaks, gaps and geometry change', () => {
    const cache = {x: new Float64Array(0), columns: new Float64Array(0)};
    const out = [];
    let data = Array.from({length: 8193}, (_, i) => [i + 1, -130 + Math.sin(i) * 15]);
    for (let frame = 0; frame < 24; frame++) {
        if (frame === 8) data.length = 4096;
        if (frame === 16) data = Array.from({length: 16385}, (_, i) => [i + 1, Math.cos(i)]);
        // Mutate existing rows and replace others. Identity cannot validate X
        // or Y; nearby grid changes can move a narrow peak to another column.
        data[2048][0] = 2049 + frame % 3 * .3;
        data[2048][1] = 100 + frame;
        data[2049] = [2050, -200 - frame];
        data[2051][1] = frame % 2 ? NaN : -100;
        const [from, to] = frame % 4 === 3 ? [2040.5, 2060.5] : [1, data.length];
        const width = [1, 80, 1000][Math.floor(frame / 4) % 3];
        const log = frame < 12;
        const expected = OriginalPlotBasics.reduceSpectrum(data, from, to, width, log);
        const actual = PlotBasics.reduceSpectrum(data, from, to, width, log, out, cache);
        assert.deepEqual(Array.from(actual), Array.from(expected), `spectrum frame ${frame}`);
        const basics = reducer(width); basics.log_x = log;
        const uncached = Array.from(basics.decimateToCanva(data, from, to, []));
        const cached = basics.decimateToCanva(data, from, to, out, cache);
        assert.deepEqual(Array.from(cached), uncached, `PNA frame ${frame}`);
        // Exercise the actual reuse branch with changed Y on a stable grid.
        data[2048][1] += 5;
        assert.deepEqual(Array.from(basics.decimateToCanva(data, from, to, out, cache)),
            Array.from(basics.decimateToCanva(data, from, to, [])), `PNA warm frame ${frame}`);
    }
});

test('logarithmic column cache skips transforms only when each coordinate is unchanged', () => {
    const data = Array.from({length: 4096}, (_, i) => [i + 1, Math.sin(i)]);
    const cache = {x: new Float64Array(0), columns: new Float64Array(0)};
    let calls = 0;
    const log10 = vm.runInContext('Math.log10', context);
    context.countedLog10 = x => {calls++; return log10(x);};
    vm.runInContext('Math.log10 = countedLog10', context);
    try {
        PlotBasics.reduceSpectrum(data, 1, 4096, 80, true, [], cache);
        assert.equal(calls, data.length + 2);
        calls = 0;
        PlotBasics.reduceSpectrum(data, 1, 4096, 80, true, [], cache);
        assert.equal(calls, 2, 'only the range endpoints need new transforms');
        calls = 0; data[2048][0] += .25;
        PlotBasics.reduceSpectrum(data, 1, 4096, 80, true, [], cache);
        assert.equal(calls, 3, 'a changed coordinate is recalculated');
    } finally { context.originalLog10 = log10; vm.runInContext('Math.log10 = originalLog10', context); }
});

function reducer(width = 80) {
    return Object.assign(Object.create(PlotBasics.prototype), {
        _decimated: [], log_x: true, plot_placeholder: {width: () => width}
    });
}
test('logarithmic reduction keeps peaks, troughs, gaps and edge neighbors; deep zoom restores every bin', () => {
    const data = Array.from({length: 16385}, (_, i) => [i + 1, -130]);
    data[2048][1] = -80; data[2049][1] = -160; data[8192][1] = NaN;
    const basics = reducer();
    const reduced = basics.decimateToCanva(data, 100, 16000).slice();
    for (const bin of [2048, 2049]) assert.ok(reduced.includes(data[bin]));
    assert.ok(reduced.some(([x, y]) => x === data[8192][0] && Number.isNaN(y)));
    assert.equal(reduced[0], data[98]); assert.equal(reduced.at(-1), data[16000]);
    for (let i = 1; i < reduced.length; i++) assert.ok(reduced[i][0] > reduced[i - 1][0]);
    const sparse = basics.decimateToCanva(data, 2000.5, 2010.5);
    assert.deepEqual(Array.from(sparse), data.slice(1999, 2011));
    assert.equal(data.length, 16385);
});

test('resizing a cached spectrum requests fresh reduction while retaining its selected axes', () => {
    let width = 600, height = 400;
    const range = {from: 200, to: 500};
    const basics = Object.assign(Object.create(PlotBasics.prototype), {
        batchedLines: true, reset_range: false, plot: {}, range_x: range,
        drawnWidth: width, drawnHeight: height,
        plot_placeholder: {width: () => width, height: () => height}
    });
    assert.equal(basics.needsRedraw(), false);
    width = 1200; height = 800;
    assert.equal(basics.needsRedraw(), true);
    assert.equal(basics.range_x, range);
    context.window = {devicePixelRatio:1};
    basics.drawnWidth = width; basics.drawnHeight = height;
    basics.drawnPixelRatio = 1; basics.reset_range = false;
    assert.equal(basics.needsRedraw(), false);
    context.window.devicePixelRatio = 2;
    assert.equal(basics.needsRedraw(), true, 'moving to another display requests a fresh backing canvas');
    assert.equal(basics.range_x, range);
});
