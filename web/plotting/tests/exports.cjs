const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {launch, load, root, ts} = require('../benchmark/harness.cjs');
(async () => {
    const browser = await launch();
    try {
        const {page, errors} = await load(browser, 'owned', 2);
        for (const source of ['web/fft/export-file/export-file.ts', 'web/phase-noise/export-file/export-file.ts']) {
            await page.addScriptTag({content: ts.transpileModule(fs.readFileSync(path.join(root, source), 'utf8'),
                {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText});
        }
        const result = await page.evaluate(async () => {
            async function check(pna) {
                setup(pna ? 1 : 0);
                basics.setVisibleRangeX(4000, 4100); render();
                document.body.insertAdjacentHTML('beforeend', '<button class="export-data"></button><button class="export-plot"></button>');
                const status = {fs:250e6, channel:0, window_index:1, clkIndex:'1', dds_freq:[0,0]};
                const spectrum = {frameStatus:status, frameReceivedAt:'2026-10-08T10:00:00Z', yLabel:'dB', plotBasics:basics,
                    plot_data:data[0], reference_data:frames[0][1], average_data:data[2], maximum_data:data[3],
                    referenceStatus:status, referenceParameters:status, referenceReceivedAt:'2026-10-08T09:00:00Z',
                    smooth_plot_data:data[2], reference_smooth_data:frames[0][1], phase_psd:new Float32Array(spec.bins).fill(1), referencePSD:new Float32Array(spec.bins).fill(2)};
                const sourceBefore = JSON.stringify([spectrum.plot_data, spectrum.reference_data]);
                let exporter;
                if (pna) {
                    class TestExport extends PnaExportFile {
                        metadata() {return ['Offset (Hz),Power (dB),Smoothed,PSD'];}
                        frameLabel() {return 'Test acquisition';}
                    }
                    exporter = new TestExport(document,spectrum);
                } else exporter = new ExportFile(document,spectrum);
                let resolve;
                const nextDownload = () => new Promise(r => {resolve = r;});
                exporter.download = blob => resolve(blob);
                const csvPromise = nextDownload(); exporter.exportData(); const csv = await (await csvPromise).text();
                const pngPromise = nextDownload(); exporter.exportPlot(); const blob = await pngPromise;
                const bitmap = await createImageBitmap(blob);
                const canvas = basics.plot.getCanvas();
                const image = document.createElement('canvas'); image.width=bitmap.width;image.height=bitmap.height;
                const ctx = image.getContext('2d'); ctx.drawImage(bitmap,0,0);
                const original = canvas.getContext('2d').getImageData(0,0,canvas.width,canvas.height).data;
                // Locate the original plot by its opaque trace/grid pixels. FFT
                // has a fixed header; PNA wraps metadata according to width.
                let matched = false;
                for (let top=60;top<400 && !matched;top+=2) {
                    if(top+canvas.height>image.height) break;
                    const exported = ctx.getImageData(0,top,canvas.width,canvas.height).data;
                    let tested=0, failed=0;
                    for(let i=0;i<original.length;i+=4) if(original[i+3]===255) {
                        tested++; if(original[i]!==exported[i] || original[i+1]!==exported[i+1] || original[i+2]!==exported[i+2]) failed++;
                    }
                    if(tested>1000 && failed===0) matched=true;
                }
                bitmap.close();
                return {csv, bins:spec.bins, width:image.width, sourceWidth:canvas.width, height:image.height,
                    sourceHeight:canvas.height, sourceUnchanged:JSON.stringify([spectrum.plot_data,spectrum.reference_data])===sourceBefore,
                    matched, first:spectrum.plot_data[0].join(','), last:spectrum.plot_data.at(-1).join(',')};
            }
            return [await check(false), await check(true)];
        });
        for (const row of result) {
            assert.ok(row.csv.includes(row.first)); assert.ok(row.csv.includes(row.last));
            assert.ok(row.csv.includes('Reference trace'));
            assert.ok(row.csv.split('\n').length > row.bins * 2, 'CSV contains all source/reference bins outside the deep zoom');
            assert.equal(row.width, row.sourceWidth); assert.ok(row.height > row.sourceHeight);
            assert.equal(row.sourceUnchanged, true); assert.equal(row.matched, true, 'PNG preserves every opaque backing-canvas pixel at DPR 2');
        }
        assert.ok(result[0].csv.includes('Average (1 s linear power EMA)')); assert.ok(result[0].csv.includes('Max hold'));
        assert.deepEqual(errors, []); await page.close();
        console.log('Actual FFT/PNA exporters: full-bin CSV, references/overlays and full-DPI PNG: PASS');
    } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
