/* The worker owns the raw record and WebAssembly workspace. Only the compact
   complex transfer response crosses back into the instrument's UI. */
importScripts('app.js');
const api = globalThis.VNA, protocol = api.protocol;
let cancelled = false, active = false, processor, referenceCache, lutSize = 0;
// Yield to cancellation messages without the nested-timer minimum delay.
const channel = new MessageChannel();
let resume;
channel.port1.onmessage = () => { const next = resume; resume = null; next(); };
const pause = () => new Promise(resolve => { resume = resolve; channel.port2.postMessage(null); });
function check() { if (cancelled) throw new Error('Sweep cancelled.'); }
function progress(stage, fraction) { self.postMessage({type: 'progress', stage, fraction}); }

async function prepare(n) {
    if (!processor) {
        const response = await fetch('chirp-processor.wasm');
        if (!response.ok) throw new Error('The chirp processor is missing. Rebuild and install the instrument.');
        processor = (await WebAssembly.instantiate(await response.arrayBuffer())).instance.exports;
    }
    const required = processor.layout(n);
    if (!required) throw new Error('Unsupported capture size.');
    const extra = Math.ceil((required - processor.memory.buffer.byteLength) / 65536);
    if (extra > 0) {
        try { processor.memory.grow(extra); }
        catch { throw new Error('Insufficient browser memory. Close other tabs and retry on a desktop browser.'); }
    }
    if (lutSize !== n) {
        const lut = new Int16Array(processor.memory.buffer, processor.lut_address(), 65536);
        for (let i = 0; i < lut.length; ++i) lut[i] = Math.round(32766 * Math.sin(2 * Math.PI * i / 65536));
        lutSize = n;
    }
    return processor;
}

async function transform(p, n, stage, from, to, adc = false) {
    const chunk = 1 << 20, m = n / 2, stages = Math.log2(m);
    for (let first = 0; first < m; first += chunk) {
        check();
        if (adc) p.adc_fft_block(first, chunk);
        else p.reorder_block(first, chunk);
        progress(stage, from + (to - from) * .12 * Math.min(1, (first + chunk) / m)); await pause();
    }
    const prefix = p.fft_prefix_length(), prefixStages = Math.log2(prefix);
    for (let first = 0; first < m; first += chunk) {
        check(); p.fft_prefix_block(first, chunk, adc ? 1 : 0);
        progress(stage, from + (to - from) * (.12 + .88 * prefixStages / stages * Math.min(1, (first + chunk) / m))); await pause();
    }
    for (let length = prefix * 2, step = prefixStages; length <= m;) {
        check();
        const paired = length * 2 <= m, last = paired ? length * 2 : length;
        const angle = -2 * Math.PI / last;
        if (paired) p.fft_stage_pair(last, Math.cos(angle), Math.sin(angle));
        else p.fft_stage(last, Math.cos(angle), Math.sin(angle));
        step += paired ? 2 : 1; length = last * 2;
        progress(stage, from + (to - from) * (.12 + .88 * step / stages)); await pause();
    }
}
function readBins(p, bins, n) {
    return bins.map(k => {
        const angle = 2 * Math.PI * k / n, c = Math.cos(angle), s = Math.sin(angle);
        return [p.bin_real(k, c, s), p.bin_imag(k, c, s)];
    });
}

self.onmessage = async event => {
    if (event.data.type === 'cancel') { cancelled = true; return; }
    if (event.data.type !== 'measure' || active) return;
    active = true; cancelled = false;
    let client, driver, token = 0, outcome;
    const started = performance.now(), timing = {};
    try {
        const settings = api.VNAModel.validateSettings(event.data.settings);
        const n = api.VNAModel.recordSamples(settings);
        progress('Preparing processor', 0);
        const p = await prepare(n); check();
        timing.prepare = performance.now() - started;
        let phase = performance.now();
        client = new api.Client(event.data.host, 1, () => { cancelled = true; });
        await client.init(); check();
        driver = new api.VNADriver(client);
        token = await driver.begin(settings);
        self.postMessage({type: 'token', token});
        const timestamp = new Date().toISOString();
        const plan = await driver.plan(token);
        if (plan.length !== protocol.planWords || plan[0] === 0) throw new Error('The FPGA sweep plan is incomplete.');
        if (await driver.recordSamples(token) !== n || plan[0] > n - 1024)
            throw new Error('The instrument returned an incompatible ADC record length.');
        progress('Capturing chirp', .03);
        const deadline = Date.now() + 6000;
        while (true) {
            check();
            const status = await driver.status(token);
            if (status === 1) break;
            if (status) throw new Error({2: 'ADC FIFO overflow. Retry the sweep.',
                3: 'ADC clipping detected. Reduce source level before retrying.',
                4: 'DMA transfer failed. Restart the instrument.', 5: 'Acquisition lease expired.',
                6: 'Acquisition timed out.', 7: 'The sampling clock changed during acquisition.'}[status] || 'Unknown acquisition error.');
            if (Date.now() > deadline) throw new Error('Acquisition timed out.');
            await new Promise(resolve => setTimeout(resolve, 15));
        }
        timing.capture = performance.now() - phase;
        phase = performance.now();
        const raw = new Uint8Array(p.memory.buffer, p.adc_address(), n * 2);
        progress('Downloading record', .06);
        const blockCount = raw.byteLength / protocol.packetBytes;
        for (let index = 0; index < blockCount; index += protocol.batchBlocks) {
            check();
            const count = Math.min(protocol.batchBlocks, blockCount - index);
            const block = await driver.blocks(token, index, count);
            check();
            if (block.byteLength !== count * protocol.packetBytes) throw new Error('Incomplete ADC record. The sweep was discarded.');
            raw.set(new Uint8Array(block.buffer, block.byteOffset, block.byteLength), index * protocol.packetBytes);
            progress('Downloading record', .06 + .24 * (index + count) / blockCount);
        }
        if (!await driver.release(token)) throw new Error('Acquisition ownership was lost.');
        token = 0; client.exit(); client = null;
        self.postMessage({type: 'token', token: 0});
        timing.download = performance.now() - phase;
        phase = performance.now();
        const chunk = 1 << 20;
        const bins = api.VNAModel.frequencyBins(settings, n);
        const key = JSON.stringify([n, api.VNAModel.FS, api.VNAModel.SOURCE_SHIFT, Array.from(plan)]);
        timing.referenceCached = referenceCache?.key === key && bins.every(k => referenceCache.bins.has(k));
        if (timing.referenceCached) {
            progress('Reference ready', .70);
        } else {
            p.set_coefficient(plan[2], plan[3]);
            p.set_source_shift(api.VNAModel.SOURCE_SHIFT);
            for (let i = 0; i < 16; ++i) p.seed(i, plan[4 + 2*i], plan[5 + 2*i]);
            for (let first = 0; first < n; first += chunk) {
                check(); p.reference_block(first, chunk, plan[0], plan[1]);
                progress('Reconstructing reference', .30 + .16 * Math.min(1, (first + chunk) / n)); await pause();
            }
            await transform(p, n, 'Transforming reference', .46, .70);
            // Presets are subsets of the densest grid. Cache that grid and the
            // requested bins so point-count changes can reuse the same source
            // transform. Custom grids remain bounded to two compact grids.
            const referenceBins = Array.from(new Set([
                ...api.VNAModel.frequencyBins({...settings, points: api.VNAModel.MAX_POINTS}, n), ...bins]));
            const spectrum = readBins(p, referenceBins, n);
            check(); referenceCache = {key, bins: new Map(referenceBins.map((k, i) => [k, spectrum[i]]))};
        }
        const x = bins.map(k => referenceCache.bins.get(k));
        timing.reference = performance.now() - phase;
        phase = performance.now();
        await transform(p, n, 'Transforming response', .70, .98, true);
        const y = readBins(p, bins, n);
        timing.response = performance.now() - phase;
        const threshold = Math.max(...x.map(v => v[0]*v[0] + v[1]*v[1])) * 1e-12;
        const real = [], imag = [];
        for (let i = 0; i < bins.length; ++i) {
            const power = x[i][0]*x[i][0] + x[i][1]*x[i][1];
            real.push(power > threshold ? (y[i][0]*x[i][0] + y[i][1]*x[i][1]) / power : null);
            imag.push(power > threshold ? (y[i][1]*x[i][0] - y[i][0]*x[i][1]) / power : null);
        }
        check();
        const frame = api.VNAModel.validateFrame({source: 'hardware', settings, timestamp,
            recordSamples: n, frequency: bins.map(k => k * api.VNAModel.FS / n), real, imag});
        timing.total = performance.now() - started; timing.bytes = n * 2;
        outcome = {type: 'result', frame, timing};
    } catch (error) {
        outcome = {type: cancelled ? 'cancelled' : 'error', message: error.message || String(error)};
    } finally {
        if (token && driver) { try { await driver.release(token); } catch {} }
        client?.exit(); active = false;
        if (outcome) self.postMessage(outcome);
    }
};
