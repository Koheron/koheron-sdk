// Actual Canvas 2D and DOM events in Chrome; no simulated canvas rasterizer.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {launch, load, root} = require('../benchmark/harness.cjs');
(async () => {
    const browser = await launch();
    fs.mkdirSync(path.join(root, 'tmp/plotting/screenshots'), {recursive: true});
    try {
        for (const scale of [1, 2]) {
            const pages = {};
            for (const variant of ['original', 'owned']) {
                pages[variant] = await load(browser, variant, scale);
                await pages[variant].page.evaluate(() => {
                    window.recording = [];
                    const proto = CanvasRenderingContext2D.prototype;
                    const colors = ['#019cd5', '#a178b5', '#006400', '#bb8000'];
                    const begin = proto.beginPath, move = proto.moveTo, line = proto.lineTo, stroke = proto.stroke;
                    proto.beginPath = function() { this._segments = []; this._point = null; return begin.apply(this, arguments); };
                    proto.moveTo = function(x,y) { this._point = [x,y]; return move.apply(this, arguments); };
                    proto.lineTo = function(x,y) {
                        if (this._point) this._segments.push([...this._point, x,y]);
                        this._point = [x,y]; return line.apply(this, arguments);
                    };
                    proto.stroke = function() {
                        if (this.canvas.classList.contains('flot-base') && colors.includes(this.strokeStyle) && this.lineWidth > 0)
                            recording.push({color: this.strokeStyle, width: this.lineWidth, segments: this._segments.slice()});
                        return stroke.apply(this, arguments);
                    };
                    window.capture = function(index) {
                        setup(index); recording.length = 0; render(7);
                        const p = basics.plot;
                        return {paths: recording, series: p.getData().map(s => ({color: s.color, width: s.lines.lineWidth,
                            data: s.data, points: s.datapoints.points})), offset: p.getPlotOffset(),
                            axes: {x: [p.getAxes().xaxis.min, p.getAxes().xaxis.max], y: [p.getAxes().yaxis.min, p.getAxes().yaxis.max]},
                            image: (() => {
                                const bytes = p.getCanvas().getContext('2d').getImageData(0, 0, p.getCanvas().width, p.getCanvas().height).data;
                                const chunks = [];
                                for (let i = 0; i < bytes.length; i += 16384) chunks.push(String.fromCharCode(...bytes.subarray(i, i + 16384)));
                                return btoa(chunks.join(''));
                            })()};
                    };
                });
            }
            for (let i = 0; i < 6; i++) {
                const original = await pages.original.page.evaluate(i => capture(i), i);
                const owned = await pages.owned.page.evaluate(i => capture(i), i);
                assert.deepEqual(owned.offset, original.offset, 'same plot layout');
                assert.deepEqual(owned.axes, original.axes, 'same linear/logarithmic axis ranges');
                assert.deepEqual(owned.series, original.series, 'same samples, gaps, line widths and trace colors');
                const segments = result => new Set(result.paths.flatMap(p => p.segments.map(s =>
                    JSON.stringify([p.color, p.width, ...s.map(x => Math.round(x * 1e6) / 1e6)]))));
                assert.deepEqual(segments(owned), segments(original), 'same clipped segments, including peaks, troughs and gap neighbours');
                if ([0,1,4].includes(i)) assert.ok(Math.max(...owned.paths.map(p => p.segments.length)) <= 32, 'bounded noise path complexity');
                const before = Buffer.from(original.image, 'base64'), after = Buffer.from(owned.image, 'base64');
                assert.equal(after.length, before.length);
                let different = 0;
                for (let pixel = 0; pixel < before.length; pixel += 4) {
                    if ([0,1,2,3].some(c => Math.abs(before[pixel+c] - after[pixel+c]) > 8)) different++;
                }
                const fraction = different / (before.length / 4);
                assert.ok(fraction < .001, `Canvas image drift ${fraction}: less than 0.1% pixels beyond 8 levels`);
                console.log(`Canvas equivalence: ${i} DPR ${scale}, ${(fraction * 100).toFixed(4)}% differing pixels`);
                if (scale === 1) await pages.owned.page.screenshot({path: path.join(root, `tmp/plotting/screenshots/scenario-${i}.png`)});
            }
            const changedGrids = [];
            for (const variant of ['original', 'owned']) changedGrids.push(await pages[variant].page.evaluate(() => {
                setup(1); render(7); // warm the owned projection cache
                for (const trace of frames[7]) {
                    for (let i = 1; i < trace.length - 1; i++) trace[i][0] += .75;
                    trace[4096][0] += .4; trace[4096][1] = -70;
                    trace[4097] = [4098.2, -180];
                    trace[6105][1] = -90; trace[6200][1] = NaN;
                }
                frames[0][1][4096][0] += .3; // captured references can change too
                recording.length = 0; render(7);
                return {paths: recording, series: basics.plot.getData().map(s => ({data:s.data, points:s.datapoints.points}))};
            }));
            assert.deepEqual(changedGrids[1].series, changedGrids[0].series, 'mutated and replaced grid rows cannot reuse stale columns or extrema');
            const changedSegments = r => new Set(r.paths.flatMap(p => p.segments.map(s => JSON.stringify([p.color,...s.map(x => Math.round(x * 1e6) / 1e6)]))));
            assert.deepEqual(changedSegments(changedGrids[1]), changedSegments(changedGrids[0]), 'updated grid/gap geometry remains identical');
            const released = await pages.owned.page.evaluate(() => {
                basics.reset_range = true;
                basics.redraw(frames[7][0], spec.bins, [], 'Power', () => {}, undefined, true);
                const active = basics._columnCaches.length;
                basics.disableDecimation();
                basics.redraw(frames[7][0], spec.bins, [], 'Power', () => {}, undefined, true);
                return {active, disabled:basics._columnCaches.length};
            });
            assert.deepEqual(released, {active:1, disabled:0}, 'removed traces and disabled reduction release projection storage');
            // Deep Y zoom exercises data-space clipping before a log transform.
            const records = [];
            for (const variant of ['original', 'owned']) records.push(await pages[variant].page.evaluate(() => {
                setup(1); basics.range_y = {from: -131, to: -130}; basics.reset_range = true;
                recording.length = 0; render();
                const p = basics.plot;
                return {paths: recording, width: p.width(), height: p.height()};
            }));
            const normalize = r => new Set(r.paths.flatMap(p => p.segments.map(s => JSON.stringify(s.map(x => Math.round(x * 1e5) / 1e5)))));
            assert.deepEqual(normalize(records[1]), normalize(records[0]), 'extreme log X / narrow Y clipping retains geometry');
            for (const r of records) for (const p of r.paths) for (const [x0,y0,x1,y1] of p.segments) {
                assert.ok(x0 >= -1e-5 && x1 <= r.width + 1e-5 && y0 >= -1e-5 && y0 <= r.height + 1e-5 && y1 >= -1e-5 && y1 <= r.height + 1e-5);
            }
            // Every pair of interior, boundary, side and corner points in both
            // directions; gaps, re-entry and direct normalized-buffer edits.
            // Compare to the original clipper, including nonlinear transforms
            // and the ordinary step/fill/shadow paths.
            for (const mode of ['plain', 'batch', 'steps', 'fill', 'shadow', 'log']) {
                const clipped = [];
                for (const variant of ['original', 'owned']) clipped.push(await pages[variant].page.evaluate(mode => {
                    basics.plot.shutdown();
                    const el = $('<div style="width:400px;height:200px">').appendTo('body');
                    const positions = [-2, 1, 5.5, 10, 12].flatMap(x => [-2, 1, 5.5, 10, 12].map(y => [x,y]));
                    const data = [];
                    for (const a of positions) for (const b of positions) data.push(a, b, null);
                    data.push(...positions, [5,5], [5,5], [10,10], [12,12], [2,2], [3,NaN], [4,4], [6,6]);
                    for (const extreme of [-Number.MAX_VALUE, -1e100, Number.MIN_VALUE, 1e100, Number.MAX_VALUE]) {
                        data.push(null, [extreme,5], [5,extreme], [5,5], [extreme,extreme]);
                    }
                    const axis = {min:1, max:10};
                    if (mode === 'log') {axis.transform = Math.log10; axis.inverseTransform = v => 10 ** v;}
                    const plot = $.plot(el, [{data, color:'#019cd5'}], {grid:{show:false}, xaxis:axis, yaxis:axis,
                        series:{reuseDatapoints:true, shadowSize:mode === 'shadow' ? 4 : 0,
                            lines:{show:true, lineWidth:1, batchSize:mode === 'batch' ? 32 : 0,
                                steps:mode === 'steps', fill:mode === 'fill'}}});
                    const capture = () => {
                        recording.length = 0; plot.draw();
                        return recording.flatMap(p => p.segments.map(s => [p.color,p.width,...s.map(v => Math.round(v * 1e6) / 1e6)]));
                    };
                    const before = capture();
                    const dp = plot.getData()[0].datapoints;
                    dp.points[0] = 5; dp.points[1] = 5;
                    dp.points[dp.pointsize] = 20; dp.points[dp.pointsize + 1] = -10;
                    const after = capture();
                    plot.shutdown(); el.remove(); return {before,after};
                },mode));
                for (const step of ['before','after']) assert.deepEqual(
                    new Set(clipped[1][step].map(JSON.stringify)), new Set(clipped[0][step].map(JSON.stringify)),
                    `all clipping regions: ${mode}, ${step}, DPR ${scale}`);
            }
            for (const {page, errors} of Object.values(pages)) { assert.deepEqual(errors, []); await page.close(); }
        }
        const {page, errors} = await load(browser, 'owned', 2);
        await page.evaluate(() => setup(0));
        // Trusted mouse events: hover and click report the exact source peak.
        const peak = await page.evaluate(() => {
            const p = basics.plot.pointOffset({x: 4097, y: -72});
            const box = document.getElementById('plot-placeholder').getBoundingClientRect();
            return {x: box.left + p.left, y: box.top + p.top};
        });
        await page.mouse.move(peak.x, peak.y); await page.mouse.click(peak.x, peak.y);
        assert.deepEqual(await page.evaluate(() => Array.from(basics.clickDatapoint)), [4097, -72]);
        assert.match(await page.locator('#hover-datapoint').textContent(), /4097\.00,-72\.00/);
        await page.evaluate(() => { render(10); });
        assert.deepEqual(await page.evaluate(() => Array.from(basics.clickDatapoint)), [4097, -72], 'cursor still uses full data on redraw');
        // Drag-selection, double-click reset, Alt-wheel X and Shift-wheel Y.
        await page.mouse.move(220, 180); await page.mouse.down(); await page.mouse.move(650, 370, {steps: 8}); await page.mouse.up();
        assert.equal(await page.evaluate(() => basics.needsRedraw()), true);
        await page.evaluate(() => render());
        assert.ok(await page.evaluate(() => basics.getRangeX().to - basics.getRangeX().from < spec.bins));
        await page.mouse.dblclick(400, 240); await page.evaluate(() => render());
        assert.deepEqual(await page.evaluate(() => basics.getRangeX()), {from: 1, to: 16385});
        await page.keyboard.down('Alt'); await page.mouse.wheel(0, -80); await page.keyboard.up('Alt');
        await page.waitForFunction(() => basics.reset_range); await page.evaluate(() => render());
        assert.ok(await page.evaluate(() => basics.getRangeX().to - basics.getRangeX().from < spec.bins));
        const beforeY = await page.evaluate(() => basics.range_y.to - basics.range_y.from);
        await page.keyboard.down('Shift'); await page.mouse.wheel(0, -80); await page.keyboard.up('Shift');
        await page.waitForFunction(() => basics.reset_range); await page.evaluate(() => render());
        assert.ok(await page.evaluate(() => basics.range_y.to - basics.range_y.from) < beforeY);
        // Resize a paused plot, then recompute retained spectrum reductions.
        const range = await page.evaluate(() => basics.getRangeX());
        await page.evaluate(() => { document.getElementById('plot-placeholder').style.width = '700px'; });
        await page.waitForFunction(() => basics.plot.getCanvas().width === 1400);
        assert.equal(await page.evaluate(() => basics.needsRedraw()), true);
        await page.evaluate(() => render());
        assert.deepEqual(await page.evaluate(() => basics.getRangeX()), range);
        assert.equal(await page.evaluate(() => basics.needsRedraw()), false);
        await page.evaluate(() => { const el = document.getElementById('plot-placeholder'); el.style.display = 'none'; });
        await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
        await page.evaluate(() => { const el = document.getElementById('plot-placeholder'); el.style.width = '850px'; el.style.display = 'block'; });
        await page.waitForFunction(() => basics.plot.getCanvas().width === 1700);
        // Normalization compatibility (including coercion, gap autoscaling and
        // shape-changing updates), frozen caller input and plugin fallbacks.
        const normalization = await page.evaluate(() => {
            const data = [[1, 2], [2, NaN], [3, null], ['4','5'], null, [Infinity, 4], [6, -Infinity], [7, 8]];
            const el = $('<div style="width:400px;height:200px">').appendTo('body');
            const input = Object.freeze({data, label:'Frozen'});
            const regular = $.plot(el, [input], {series: {lines:{show:true}, shadowSize:0}});
            const expected = [...regular.getData()[0].datapoints.points]; const expectedAxes = regular.getAxes();
            const expectedBounds = [expectedAxes.xaxis.datamin, expectedAxes.xaxis.datamax, expectedAxes.yaxis.datamin, expectedAxes.yaxis.datamax];
            const fast = $.plot(el, [input], {series: {reuseDatapoints:true, lines:{show:true}, shadowSize:0}});
            const actual = [...fast.getData()[0].datapoints.points]; const actualAxes = fast.getAxes();
            const actualBounds = [actualAxes.xaxis.datamin, actualAxes.xaxis.datamax, actualAxes.yaxis.datamin, actualAxes.yaxis.datamax];
            const buffer = fast.getData()[0].datapoints.points;
            fast.setData([{data:[[1,9], [2,10]]}]);
            const shrunk = [...fast.getData()[0].datapoints.points];
            const reused = fast.getData()[0].datapoints.points === buffer;
            fast.setData([{data:[[1,1],[2,2],[3,3]], lines:{steps:true}}]);
            const steps = [...fast.getData()[0].datapoints.points];
            let calls = 0;
            fast.hooks.processDatapoints.push((p,s,dp) => {calls++; dp.points[1]=99;});
            fast.setData([{data:[[1,1],[2,2]]}]);
            const pluginValue = fast.getData()[0].datapoints.points[1];
            const pluginNewBuffer = fast.getData()[0].datapoints.points !== buffer;
            fast.shutdown(); el.remove();
            return {expected,actual,expectedBounds,actualBounds,shrunk,reused,steps,calls,pluginValue,pluginNewBuffer};
        });
        assert.deepEqual(normalization.actual, normalization.expected);
        assert.deepEqual(normalization.actualBounds, normalization.expectedBounds);
        assert.deepEqual(normalization.shrunk, [1,9,2,10]); assert.equal(normalization.reused, true);
        assert.deepEqual(normalization.steps, [1,1,2,1,2,2,3,2,3,3]);
        assert.equal(normalization.calls, 1); assert.equal(normalization.pluginValue, 99); assert.equal(normalization.pluginNewBuffer, true);
        // The finite-number shortcut must preserve the generic normalizer's
        // coercion and autoscale semantics through reused, shrinking buffers.
        const numericCases = await page.evaluate(() => {
            const elements = [0,1].map(() => $('<div style="width:400px;height:200px">').appendTo('body'));
            const options = {grid:{show:false}, xaxis:{min:-10,max:10}, yaxis:{min:-10,max:10},
                series:{lines:{show:false}, points:{show:false}, shadowSize:0}};
            const regular = $.plot(elements[0], [], options);
            const fast = $.plot(elements[1], [], {...options,series:{...options.series,reuseDatapoints:true}});
            const cases = [
                [[0,Number.MIN_VALUE], [1,-Number.MIN_VALUE], [2,Number.MAX_VALUE/2], [3,-Number.MAX_VALUE/2]],
                [[1,2], ['2','3'], [true,false], [null,4], [5,undefined], null, [6,NaN], [Infinity,7],
                    [-Infinity,8], [9,Infinity], [10,-Infinity], [Number.MAX_VALUE,11], [-Number.MAX_VALUE,-12],
                    [13,Number.MAX_VALUE], [14,-Number.MAX_VALUE]],
                [[2,3]],
                Array.from({length:128}, (_,i) => i % 17 ? [i,Math.sin(i)] : [i,NaN]),
                [null, [3,4], null]
            ];
            const snapshot = plot => {
                const axes = plot.getAxes();
                return {points:[...plot.getData()[0].datapoints.points],
                    bounds:[axes.xaxis.datamin,axes.xaxis.datamax,axes.yaxis.datamin,axes.yaxis.datamax]};
            };
            const result = [];
            for (const data of cases) {
                data.forEach(row => {if (row) Object.freeze(row);}); Object.freeze(data);
                regular.setData([Object.freeze({data})]); fast.setData([Object.freeze({data})]);
                result.push({expected:snapshot(regular),actual:snapshot(fast)});
            }
            regular.shutdown(); fast.shutdown(); elements.forEach(el => el.remove());
            return result;
        });
        for (const row of numericCases) assert.deepEqual(row.actual,row.expected,
            'finite, subnormal, coerced, missing and sentinel coordinates retain normalization/bounds');
        const batchedNumbers = await page.evaluate(() => {
            const el = $('<div style="width:400px;height:200px">').appendTo('body');
            const plot = $.plot(el, [{data:[[1,2],[2,Infinity],[3,-Infinity],[Infinity,4],[-Infinity,-3],
                [5,NaN],[6,undefined],[null,7],['7','8'],[Number.MAX_VALUE,9],[-Number.MAX_VALUE,-9],
                [8,Number.MAX_VALUE],[9,-Number.MAX_VALUE]]}],
                {grid:{show:false},xaxis:{min:-10,max:10},yaxis:{min:-10,max:10},
                    series:{reuseDatapoints:true,lines:{show:false,batchSize:32},shadowSize:0}});
            const axes = plot.getAxes();
            const result = {points:[...plot.getData()[0].datapoints.points],
                bounds:[axes.xaxis.datamin,axes.xaxis.datamax,axes.yaxis.datamin,axes.yaxis.datamax]};
            plot.shutdown(); el.remove(); return result;
        });
        assert.deepEqual(batchedNumbers, {points:[1,2,null,null,null,null,null,null,null,null,
            null,null,null,null,null,null,7,8,Number.MAX_VALUE,9,-Number.MAX_VALUE,-9,
            8,Number.MAX_VALUE,9,-Number.MAX_VALUE],bounds:[1,9,-9,9]},
            'batched infinities are gaps; finite MAX_VALUE sentinels remain points but not bounds');
        const fallback = await page.evaluate(() => {
            const result = [];
            const element = $('<div style="width:400px;height:200px">').appendTo('body');
            for (const type of [{lines:{show:true,steps:true}}, {lines:{show:true,fill:true}}, {bars:{show:true}}]) {
                const opts = {series:{...type, shadowSize:0, reuseDatapoints:true}};
                const plot = $.plot(element, [{data:[[1,10000],[2,20000],[3,30000]]}],opts);
                const input = [{data:[null,[10,20],null]}];
                plot.setData(input);
                const axes = plot.getAxes();
                const actual = {points:[...plot.getData()[0].datapoints.points], bounds:[axes.xaxis.datamin,axes.xaxis.datamax,axes.yaxis.datamin,axes.yaxis.datamax]};
                const fresh = $.plot(element,input,opts), freshAxes = fresh.getAxes();
                const expected = {points:[...fresh.getData()[0].datapoints.points], bounds:[freshAxes.xaxis.datamin,freshAxes.xaxis.datamax,freshAxes.yaxis.datamin,freshAxes.yaxis.datamax]};
                result.push({actual,expected}); fresh.shutdown();
            }
            element.remove(); return result;
        });
        for (const row of fallback) assert.deepEqual(row.actual,row.expected,'filled, bar and stepped updates discard stale gap bounds');

        // Legacy-browser resize fallback detects element changes too; both
        // paths must release callbacks after shutdown.
        await page.evaluate(() => { window.ResizeObserver = undefined; setup(3); });
        await page.evaluate(() => { document.getElementById('plot-placeholder').style.width = '650px'; });
        await page.waitForFunction(() => basics.plot.getCanvas().width === 1300);
        const frozenWidth = await page.evaluate(() => { basics.plot.shutdown(); return basics.plot.getCanvas().width; });
        await page.evaluate(() => { document.getElementById('plot-placeholder').style.width = '900px'; });
        await page.waitForTimeout(300);
        assert.equal(await page.evaluate(() => basics.plot.getCanvas().width), frozenWidth, 'shutdown cancels fallback timers');
        assert.deepEqual(errors, []);
        await page.close();
        console.log('Browser rendering/clipping, trusted interactions, DPR, resize, normalization and plugin compatibility: PASS');
    } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
