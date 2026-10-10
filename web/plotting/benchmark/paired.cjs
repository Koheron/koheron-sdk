// Compare two stacks on one renderer thread, alternating order within each rAF.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const {launch, install, root} = require('./harness.cjs');
const arg = (key, fallback) => process.argv.find(x => x.startsWith('--' + key + '='))?.split('=')[1] || fallback;
(async () => {
    const variants = arg('variants', 'previous,owned').split(',');
    if (variants.length !== 2 || variants[0] === variants[1]) throw new Error('Choose two distinct variants');
    const frames = Number(arg('frames', 180)), rounds = Number(arg('rounds', 3));
    const browser = await launch();
    const report = {browser:browser.version(),cpu:os.cpus()[0].model,platform:`${os.platform()} ${os.release()}`,
        method:'two same-origin frames, alternating redraw order within each rAF',
        headless:true,gpu:'disabled',viewport:[2400,800],plotViewport:[1000,500],dpr:1,
        hover:false,heapSampling:false,frames,rounds,results:[],sourceSHA256:{}};
    if (os.platform() === 'linux' && fs.existsSync('/proc/self/status')) {
        const affinity = fs.readFileSync('/proc/self/status','utf8').match(/^Cpus_allowed_list:\s*(.+)$/m);
        if (affinity) report.cpuAffinity = affinity[1].trim();
    }
    for (const file of ['web/plot-basics/plot-basics.ts','web/plotting/benchmark/workload.js',
        'web/plotting/benchmark/paired.cjs','web/plotting/benchmark/harness.cjs',
        ...fs.readdirSync(path.join(root,'web/plotting/src')).map(name => 'web/plotting/src/'+name)]) {
        report.sourceSHA256[file] = crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex');
    }
    if (variants.includes('previous')) report.comparisonSnapshot = JSON.parse(fs.readFileSync(path.join(root,'tmp/plotting/comparison/snapshot.json')));
    try {
        for (let round = 0; round < rounds; round++) {
            const page = await browser.newPage({viewport:{width:2400,height:800},deviceScaleFactor:1});
            const errors = [];
            page.on('pageerror',e => errors.push(e.message));
            const body = '<!doctype html><html><body style="margin:8px;font:14px Arial"></body></html>';
            await page.setContent(`<body style="margin:0;display:flex">${[0,1].map(() =>
                `<iframe style="border:0;width:1200px;height:800px;flex:none" srcdoc='${body}'></iframe>`).join('')}</body>`);
            const children = page.frames().filter(frame => frame !== page.mainFrame());
            if (children.length !== 2) throw new Error('Expected two benchmark frames');
            const order = round % 2 ? [...variants].reverse() : variants;
            for (let i = 0; i < children.length; i++) {
                await install(children[i],order[i]);
                await children[i].evaluate(variant => {window.benchmarkVariant = variant;},order[i]);
            }
            await page.evaluate(() => {
                window.views = Array.from(document.querySelectorAll('iframe'),el => el.contentWindow);
                window.measurePair = async count => {
                    const redraw = [[],[]],delta = [],intervals = [];
                    let frame = 0,last;
                    await new Promise(resolve => {
                        function next(time) {
                            if (last !== undefined) intervals.push(time-last);
                            last = time;
                            const costs = [];
                            for (const index of (frame % 2 ? [1,0] : [0,1])) {
                                const start = performance.now(); views[index].render(frame);
                                const cost = performance.now()-start;
                                redraw[index].push(cost); costs[index] = cost;
                            }
                            delta.push(costs[0]-costs[1]);
                            if (++frame < count) requestAnimationFrame(next); else requestAnimationFrame(resolve);
                        }
                        requestAnimationFrame(next);
                    });
                    const stats = values => {
                        const mean = values.reduce((sum,v) => sum+v,0)/values.length;
                        values.sort((a,b) => a-b);
                        return {median:values[Math.floor(values.length*.5)],p95:values[Math.floor(values.length*.95)],
                            mean,max:values.at(-1),samples:values.length};
                    };
                    return {redraw:redraw.map(stats),deltaMS:stats(delta),frameIntervalMS:stats(intervals)};
                };
            });
            const scenarios = await children[0].evaluate(() => scenarios.map(s => s.name));
            for (let i = 0; i < scenarios.length; i++) {
                await page.evaluate(index => views.forEach(view => view.setup(index)),i);
                await page.evaluate(() => measurePair(30));
                const timings = await page.evaluate(count => measurePair(count),frames);
                const row = {round,scenario:scenarios[i],redrawMS:Object.fromEntries(order.map((v,j) => [v,timings.redraw[j]])),
                    deltaOrder:order,deltaMS:timings.deltaMS,frameIntervalMS:timings.frameIntervalMS};
                report.results.push(row);
                console.log(`${round+1} ${row.scenario}: ${variants.map(v => `${v} ${row.redrawMS[v].median.toFixed(2)} / ${row.redrawMS[v].p95.toFixed(2)} ms`).join('; ')}`);
                if (errors.length) throw new Error(errors.join('\n'));
            }
            await page.close();
        }
    } finally {await browser.close();}
    const output = path.resolve(root,arg('output','tmp/plotting/paired.json'));
    fs.mkdirSync(path.dirname(output),{recursive:true}); fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
    console.log(output);
})().catch(e => {console.error(e);process.exitCode=1;});
