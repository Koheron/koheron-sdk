// Deterministic RF-like fixtures. Prepared before timing; each frame updates
// the same arrays, as the instrument adapters do, rather than allocating inputs.
window.scenarios = [
    {name: 'fft-linear-4', bins: 16385, traces: 4},
    {name: 'pna-log-3', bins: 32769, traces: 3, log: true},
    {name: 'fft-deep-zoom', bins: 16385, traces: 4, deep: true},
    {name: 'scope-2', bins: 8192, traces: 2, scope: true},
    {name: 'pna-log-y', bins: 32769, traces: 2, log: true, logY: true},
    {name: 'scope-y-zoom', bins: 8192, traces: 2, scope: true, narrowY: true}
];
window.fixture = function(spec, frame = 0) {
    let seed = 123456789 + frame * 7919;
    const random = () => { seed ^= seed << 13; seed ^= seed >>> 17; seed ^= seed << 5; return (seed >>> 0) / 4294967296; };
    return Array.from({length: spec.traces}, (_, trace) => Array.from({length: spec.bins}, (_, i) => {
        let y = spec.scope ? Math.sin(i * 0.021 + trace + frame * .05) : -135 + random() * 16 + trace * 3;
        if (!spec.scope && i === 4096) y = -72 + trace;
        if (!spec.scope && i === 4097) y = -172;
        if (spec.logY) y = Math.pow(10, (y + 140) / 20);
        if (i > 6100 && i < 6120) y = NaN;
        return [i + 1, y];
    }));
};
window.setup = function(index) {
    if (window.basics?.plot) { window.basics.plot.shutdown(); }
    document.body.innerHTML = '<span id="plot-title"></span><div id="plot-placeholder" style="width:1000px;height:500px"></div>' +
        '<span id="hover-datapoint"></span><span id="click-datapoint"></span><span id="peak-datapoint"></span>';
    window.spec = scenarios[index];
    window.frames = Array.from({length: 8}, (_, frame) => fixture(spec, frame));
    window.data = frames[0];
    window.basics = new PlotBasics(document, $('#plot-placeholder'), spec.bins, 1, spec.bins,
        spec.scope ? -1.5 : spec.logY ? 0.01 : -180, spec.scope ? 1.5 : spec.logY ? 1e4 : -65, {}, '', 'Benchmark');
    basics.isPeakDetection = false;
    if (spec.log) basics.setLogX(true);
    if (spec.logY) basics.setLogY();
    if (!spec.scope) {
        if (spec.log) basics.enableDecimation(); else basics.enableSpectrumReduction();
        basics.enableBatchedLines();
    }
    if (spec.deep) basics.setVisibleRangeX(4000, 4100);
    if (spec.narrowY) basics.range_y = {from: -.05, to: .05};
    window.extras = data.slice(2).map((trace, i) => ({label: i ? 'Max hold' : 'Average', data: trace, color: i ? '#bb8000' : '#006400'}));
    window.render = function(frame = 0) {
        // Cycle prepared frames: every live/average/max-hold bin changes, while
        // the captured reference stays fixed. Fixture creation is not timed.
        data = frames[frame % frames.length];
        extras.forEach((trace, i) => { trace.data = data[i + 2]; });
        if (spec.scope) basics.redrawTwoChannels(data[0], data[1], basics.range_x, 'Channel 1', 'Channel 2', true, true, () => {});
        else basics.redraw(data[0], spec.bins, [], 'Power', () => {}, frames[0][1], true, extras);
    };
    render();
};
window.measure = async function(count = 120) {
    const redraw = [], intervals = [], handlers = [], inputToRAF = [], trustedHandlers = [], trustedInputToRAF = [];
    let frame = 0, last;
    const ph = document.getElementById('plot-placeholder');
    let arrival;
    function inputStart(event) { if (event.isTrusted) arrival = performance.now(); }
    function inputEnd(event) {
        if (!event.isTrusted) return;
        trustedHandlers.push(performance.now() - arrival);
        const stamp = event.timeStamp;
        requestAnimationFrame(() => trustedInputToRAF.push(performance.now() - stamp));
    }
    document.addEventListener('mousemove', inputStart, true);
    ph.addEventListener('mousemove', inputEnd);
    // Dispatch through Flot's actual DOM handlers. Measures handler work and
    // arrival-to-next-animation-frame, independent of automation round trips.
    function move() {
        const p = basics.plot.pointOffset({x: spec.deep ? 4050 : spec.bins / 2, y: spec.scope ? 0 : spec.logY ? 1 : -127});
        const box = ph.getBoundingClientRect();
        const t = performance.now();
        ph.querySelector('.flot-overlay').dispatchEvent(new MouseEvent('mousemove', {
            bubbles: true, clientX: box.left + p.left, clientY: box.top + p.top
        }));
        handlers.push(performance.now() - t);
        requestAnimationFrame(() => inputToRAF.push(performance.now() - t));
    }
    await new Promise(resolve => {
        function next(time) {
            if (last !== undefined) intervals.push(time - last);
            last = time;
            const t = performance.now(); render(frame); redraw.push(performance.now() - t);
            if (frame % 4 === 0) move();
            if (++frame < count) requestAnimationFrame(next); else requestAnimationFrame(resolve);
        }
        requestAnimationFrame(next);
    });
    document.removeEventListener('mousemove', inputStart, true);
    ph.removeEventListener('mousemove', inputEnd);
    const stats = xs => {
        xs.sort((a,b) => a-b);
        return {median: xs[Math.floor(xs.length * .5)], p95: xs[Math.floor(xs.length * .95)], max: xs.at(-1), samples: xs.length};
    };
    return {redrawMS: stats(redraw), frameIntervalMS: stats(intervals), hoverHandlerMS: stats(handlers), inputToRAFMS: stats(inputToRAF),
        trustedHoverHandlerMS: stats(trustedHandlers), trustedInputToRAFMS: stats(trustedInputToRAF)};
};
// Measure a real widget zoom/reset path separately from steady-state redraws.
// DOM events are synthetic here; trusted hover events are measured above.
window.measureZoom = async function(count = 12) {
    const cpu = [], latency = [];
    const ph = document.getElementById('plot-placeholder');
    for (let i = 0; i < count; i++) {
        await new Promise(resolve => requestAnimationFrame(() => {
            const box = ph.getBoundingClientRect();
            const event = i % 2 ? new MouseEvent('dblclick', {bubbles: true, clientX: box.left + 500, clientY: box.top + 250})
                : new WheelEvent('wheel', {bubbles:true, cancelable:true, altKey:true, deltaY:-80, clientX:box.left+500, clientY:box.top+250});
            const start = performance.now();
            ph.querySelector('.flot-overlay').dispatchEvent(event);
            render(i); cpu.push(performance.now() - start);
            requestAnimationFrame(() => {latency.push(performance.now() - start); resolve();});
        }));
    }
    const stats = values => { values.sort((a,b)=>a-b); return {median:values[Math.floor(values.length*.5)], p95:values[Math.floor(values.length*.95)], samples:values.length}; };
    return {zoomRedrawMS:stats(cpu), zoomToRAFMS:stats(latency)};
};
