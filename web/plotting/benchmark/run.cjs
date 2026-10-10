// npm run benchmark -- --variants=original,owned --frames=180 --rounds=3
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const {launch, load, root} = require('./harness.cjs');
const arg = (key, fallback) => process.argv.find(x => x.startsWith('--' + key + '='))?.split('=')[1] || fallback;
(async () => {
    const variants = arg('variants', 'original,owned').split(',');
    const frames = Number(arg('frames', 180)), rounds = Number(arg('rounds', 3));
    const browser = await launch();
    const report = {browser: browser.version(), cpu: os.cpus()[0].model, platform: `${os.platform()} ${os.release()}`,
        headless: true, gpu: 'disabled', viewport: [1000, 500], dpr: 1, frames, rounds, results: []};
    if (os.platform() === 'linux' && fs.existsSync('/proc/self/status')) {
        const affinity = fs.readFileSync('/proc/self/status', 'utf8').match(/^Cpus_allowed_list:\s*(.+)$/m);
        if (affinity) report.cpuAffinity = affinity[1].trim();
    }
    report.sourceSHA256 = {};
    for (const name of ['web/plot-basics/plot-basics.ts', 'web/plotting/benchmark/workload.js',
        ...fs.readdirSync(path.join(root, 'web/plotting/src')).map(name => 'web/plotting/src/' + name)]) {
        report.sourceSHA256[name] = crypto.createHash('sha256').update(fs.readFileSync(path.join(root, name))).digest('hex');
    }
    if (variants.includes('previous')) report.comparisonSnapshot = JSON.parse(fs.readFileSync(path.join(root, 'tmp/plotting/comparison/snapshot.json')));
    try {
        // Alternate execution order each round to reduce warmup/thermal bias.
        for (let round = 0; round < rounds; round++) for (const variant of (round % 2 ? [...variants].reverse() : variants)) {
            const {page, errors} = await load(browser, variant);
            const cdp = await page.context().newCDPSession(page);
            await cdp.send('Performance.enable');
            const scenarios = await page.evaluate(() => scenarios);
            for (let i = 0; i < scenarios.length; i++) {
                await page.evaluate(i => setup(i), i);
                await page.evaluate(() => measure(30)); // warm caches and JIT
                await cdp.send('HeapProfiler.collectGarbage');
                const before = await cdp.send('Performance.getMetrics');
                await cdp.send('HeapProfiler.startSampling', {samplingInterval: 16384,
                    includeObjectsCollectedByMajorGC: true, includeObjectsCollectedByMinorGC: true});
                const measurement = page.evaluate(n => measure(n), frames);
                for (let move = 0; move < 20; move++) {
                    await page.mouse.move(200 + move * 31 % 700, 220 + move % 3 * 20);
                    await new Promise(resolve => setTimeout(resolve, 33));
                }
                const timings = await measurement;
                const {profile} = await cdp.send('HeapProfiler.stopSampling');
                await cdp.send('HeapProfiler.collectGarbage');
                const after = await cdp.send('Performance.getMetrics');
                const heap = metrics => metrics.metrics.find(m => m.name === 'JSHeapUsedSize').value;
                const allocationNodes = {};
                function walk(node) {
                    allocationNodes[node.id] = node.callFrame.functionName || '(anonymous)';
                    for (const child of node.children) walk(child);
                }
                walk(profile.head);
                const allocatedByFunction = {};
                for (const sample of profile.samples) {
                    const name = allocationNodes[sample.nodeId];
                    allocatedByFunction[name] = (allocatedByFunction[name] || 0) + sample.size;
                }
                const row = {allocationHotspots: Object.entries(allocatedByFunction).sort((a,b) => b[1]-a[1]).slice(0,8), round, variant, scenario: scenarios[i].name, ...timings,
                    sampledAllocatedBytesPerFrame: profile.samples.reduce((sum, s) => sum + s.size, 0) / frames,
                    projectionCacheBytes: await page.evaluate(() => (basics._columnCaches || []).reduce((sum, cache) => sum + cache.x.byteLength + cache.columns.byteLength, 0)),
                    retainedHeapDeltaBytes: heap(after) - heap(before)};
                if (process.argv.includes('--profile') && round === 0) {
                    await cdp.send('Profiler.enable');
                    await cdp.send('Profiler.setSamplingInterval', {interval: 1000});
                    await cdp.send('Profiler.start');
                    await page.evaluate(() => measure(60));
                    const cpu = await cdp.send('Profiler.stop');
                    const profilePath = path.join(root, `tmp/plotting/${variant}-${row.scenario}.cpuprofile`);
                    fs.mkdirSync(path.dirname(profilePath), {recursive:true});
                    fs.writeFileSync(profilePath, JSON.stringify(cpu.profile));
                    row.cpuProfile = path.relative(root, profilePath);
                }
                await cdp.send('HeapProfiler.collectGarbage');
                await cdp.send('HeapProfiler.startSampling', {samplingInterval:16384,
                    includeObjectsCollectedByMajorGC:true, includeObjectsCollectedByMinorGC:true});
                Object.assign(row, await page.evaluate(() => measureZoom()));
                const zoomHeap = await cdp.send('HeapProfiler.stopSampling');
                row.zoomSampledAllocatedBytesPerOperation = zoomHeap.profile.samples.reduce((sum,s) => sum+s.size,0) / row.zoomRedrawMS.samples;
                report.results.push(row);
                console.log(`${round + 1} ${variant} ${row.scenario}: redraw ${timings.redrawMS.median.toFixed(2)} / ${timings.redrawMS.p95.toFixed(2)} ms; frame p95 ${timings.frameIntervalMS.p95.toFixed(2)} ms; sampled ${(row.sampledAllocatedBytesPerFrame/1024).toFixed(1)} KiB/frame`);
                if (errors.length) throw new Error(errors.join('\n'));
            }
            await page.close();
        }
    } finally { await browser.close(); }
    const output = path.resolve(root, arg('output', 'tmp/plotting/benchmark.json'));
    fs.mkdirSync(path.dirname(output), {recursive: true}); fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
    console.log(output);
})().catch(e => { console.error(e); process.exitCode = 1; });
