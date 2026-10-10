// Allocation control: empty hook lists, no data/grid/text or plugin work.
// This isolates dispatcher allocations; it is not an instrument FPS benchmark.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const os = require('node:os');
const {launch,load,root} = require('./harness.cjs');
const arg = (key,fallback) => process.argv.find(x => x.startsWith('--'+key+'='))?.split('=')[1] || fallback;
(async () => {
    const variants = arg('variants','previous,owned').split(',');
    const frames = Number(arg('frames',60)), draws = Number(arg('draws',100)), rounds = Number(arg('rounds',3));
    if (variants.length !== 2 || variants[0] === variants[1]) throw new Error('Choose two distinct variants');
    if (![frames,draws,rounds].every(n => Number.isInteger(n) && n > 0)) throw new Error('frames, draws and rounds must be positive integers');
    const browser = await launch();
    const report = {browser:browser.version(),cpu:os.cpus()[0].model,platform:`${os.platform()} ${os.release()}`,
        method:'empty-hook static plot, 16 KiB heap sampling including collected objects',
        headless:true,gpu:'disabled',dpr:1,plotViewport:[400,200],frames,draws,rounds,results:[],sourceSHA256:{}};
    if (os.platform() === 'linux') report.cpuAffinity = fs.readFileSync('/proc/self/status','utf8').match(/^Cpus_allowed_list:\s*(.+)$/m)?.[1].trim();
    for (const file of ['web/plotting/benchmark/hooks.cjs','web/plotting/benchmark/harness.cjs',
        'web/plot-basics/plot-basics.ts','web/plotting/benchmark/workload.js',
        ...fs.readdirSync(path.join(root,'web/plotting/src')).map(name => 'web/plotting/src/'+name)]) {
        report.sourceSHA256[file] = crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex');
    }
    if (variants.includes('previous')) report.comparisonSnapshot = JSON.parse(fs.readFileSync(path.join(root,'tmp/plotting/comparison/snapshot.json')));
    try {
        for (let round = 0; round < rounds; round++) for (const variant of (round % 2 ? [...variants].reverse() : variants)) {
            const {page,errors} = await load(browser,variant);
            await page.evaluate(() => {
                $.plot.plugins = [];
                document.body.innerHTML = '<div id="control" style="width:400px;height:200px"></div>';
                window.control = $.plot($('#control'),[{data:[],lines:{show:false}}],
                    {grid:{show:false},legend:{show:false},xaxis:{min:0,max:1},yaxis:{min:0,max:1}});
                if (Object.values(control.hooks).some(list => list.length)) throw new Error('Expected empty hook lists');
                control.getOptions().interaction.redrawOverlayInterval = -1;
                window.drawFrames = (frames,draws) => new Promise(resolve => {
                    let frame = 0;
                    function next() {
                        for (let i = 0; i < draws; i++) control.draw();
                        if (++frame < frames) requestAnimationFrame(next); else requestAnimationFrame(resolve);
                    }
                    requestAnimationFrame(next);
                });
            });
            await page.evaluate(draws => drawFrames(30,draws),draws);
            const cdp = await page.context().newCDPSession(page);
            await cdp.send('HeapProfiler.collectGarbage');
            await cdp.send('HeapProfiler.startSampling',{samplingInterval:16384,
                includeObjectsCollectedByMajorGC:true,includeObjectsCollectedByMinorGC:true});
            await page.evaluate(({frames,draws}) => drawFrames(frames,draws),{frames,draws});
            const {profile} = await cdp.send('HeapProfiler.stopSampling');
            const names = {}, bytes = {};
            function walk(node) {
                names[node.id] = node.callFrame.functionName || '(anonymous)';
                node.children.forEach(walk);
            }
            walk(profile.head);
            for (const sample of profile.samples) bytes[names[sample.nodeId]] = (bytes[names[sample.nodeId]] || 0)+sample.size;
            const row = {round,variant,totalDraws:frames*draws,
                sampledAllocatedBytesPerDraw:profile.samples.reduce((sum,s) => sum+s.size,0)/(frames*draws),
                sampledBytesByFunction:bytes};
            report.results.push(row);
            console.log(`${round+1} ${variant}: ${row.sampledAllocatedBytesPerDraw.toFixed(1)} sampled bytes/draw`);
            if (errors.length) throw new Error(errors.join('\n'));
            await page.evaluate(() => control.shutdown()); await page.close();
        }
    } finally {await browser.close();}
    const output = path.resolve(root,arg('output','tmp/plotting/hooks-control.json'));
    fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
    console.log(output);
})().catch(e => {console.error(e);process.exitCode=1;});
