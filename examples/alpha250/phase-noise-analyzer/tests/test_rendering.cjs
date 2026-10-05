const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('typescript');
const context = vm.createContext({});
vm.runInContext(ts.transpileModule(fs.readFileSync(path.join(__dirname, '../../../../web/plot-basics/plot-basics.ts'), 'utf8'),
    {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText + '\nglobalThis.PlotBasics = PlotBasics;', context);
const PlotBasics = context.PlotBasics;

function canvas() {
    const segments = [], paths = [];
    let current, count = 0, saves = 0;
    return {
        segments, paths,
        save() { saves++; }, restore() { saves--; }, translate() {}, rect() {}, clip() {},
        beginPath() { current = undefined; count = 0; },
        moveTo(x, y) { current = [x, y]; },
        lineTo(x, y) { if (current) { segments.push([...current, x, y]); count++; } current = [x, y]; },
        stroke() { paths.push(count); },
        get saves() { return saves; }
    };
}
const axes = {xaxis: {min: 1, max: 1000, p2c: x => Math.log10(x) * 100},
    yaxis: {min: -140, max: -120, p2c: y => (-120 - y) * 10}};

test('batched canvas strokes retain every visible segment, sharp extrema, and gaps', () => {
    const data = Array.from({length: 513}, (_, i) => [i + 1, -130 + Math.sin(i) * 8]);
    data[32][1] = -121; data[33][1] = -139;
    data[64][1] = NaN; data[129][1] = Infinity; data[193][1] = null;
    const raw = data.map(row => row.slice());
    const drawing = canvas();
    PlotBasics.drawBatchedLines(drawing, {...axes, data, color: '#019cd5', lines: {lineWidth: 2}}, {left: 40, top: 10}, 300, 200);
    const expected = [];
    for (let i = 1; i < data.length; i++) {
        if ([data[i - 1][1], data[i][1]].every(y => y !== null && Number.isFinite(y))) {
            expected.push([axes.xaxis.p2c(data[i - 1][0]), axes.yaxis.p2c(data[i - 1][1]),
                axes.xaxis.p2c(data[i][0]), axes.yaxis.p2c(data[i][1])]);
        }
    }
    const segments = new Set(drawing.segments.map(JSON.stringify));
    assert.deepEqual(segments, new Set(expected.map(JSON.stringify)), 'same polyline, with no invented connections across gaps');
    assert.ok(Math.max(...drawing.paths) <= 32, 'canvas path complexity stays bounded for tall jagged noise');
    assert.equal(drawing.saves, 0, 'the HiDPI canvas transform and clip state are restored');
    assert.deepEqual(data, raw, 'capture and exports keep the full source bins');
});

test('extreme zooms clip before the logarithmic transform and keep canvas coordinates bounded', () => {
    const xaxis = {min: 1, max: 100, p2c: x => Math.log10(x) * 100};
    const yaxis = {min: -131, max: -130, p2c: y => (-130 - y) * 100};
    assert.deepEqual(Array.from(PlotBasics.clipLine([1, -132], [100, -129], xaxis, yaxis)), [34, -131, 67, -130]);
    assert.equal(PlotBasics.clipLine([1, -132], [100, -132], xaxis, yaxis), undefined);
    const data = Array.from({length: 513}, (_, i) => [1 + i / 512 * 99, i % 2 ? 1e6 : -1e6]);
    const drawing = canvas();
    PlotBasics.drawBatchedLines(drawing, {xaxis, yaxis, data, color: '#019cd5', lines: {lineWidth: 2}}, {left: 0, top: 0}, 200, 100);
    assert.ok(drawing.segments.length > 500);
    for (const [x0, y0, x1, y1] of drawing.segments) {
        assert.ok(x0 >= 0 && x1 <= 200 && y0 >= 0 && y0 <= 100 && y1 >= 0 && y1 <= 100);
    }
});

test('raw, reference and smoothed series retain their Flot hit testing and line styles after drawing', () => {
    const basics = Object.assign(Object.create(PlotBasics.prototype), {options: {}});
    basics.enableBatchedLines();
    const hooks = basics.options.hooks;
    const plot = {width: () => 300, height: () => 200, getPlotOffset: () => ({left: 30, top: 10})};
    const traces = ['#019cd5', '#a178b5', '#006400'].map((color, i) => ({...axes, color,
        data: Array.from({length: 257}, (_, j) => [j + 1, -130 + Math.sin(j)]), lines: {show: true, lineWidth: i ? 1 : 2}}));
    for (const series of traces) {
        const drawing = canvas(); hooks.drawSeries[0](plot, drawing, series);
        assert.equal(series.lines.show, true, 'hover and click handling remain enabled');
        assert.equal(series.lines.lineWidth, 0, 'Flot must not draw a second copy of the same line');
        assert.ok(drawing.segments.length);
    }
    hooks.draw[0](plot);
    assert.deepEqual(traces.map(series => series.lines.lineWidth), [2, 1, 1]);
    const sparse = {...traces[0], data: [[1, -130], [2, -131]], lines: {show: true, lineWidth: 2}};
    const drawing = canvas(); hooks.drawSeries[0](plot, drawing, sparse);
    assert.equal(sparse.lines.lineWidth, 2, 'sparse, fully resolved traces keep the standard renderer');
    assert.equal(drawing.segments.length, 0);
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
});
