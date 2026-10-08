// Compare repeated range changes against a fresh original Plot, including
// automatic bounds, changing ticks/legend and the full-rebuild fallbacks.
const assert = require('node:assert/strict');
const {launch, load} = require('../benchmark/harness.cjs');
(async () => {
    const browser = await launch();
    try {
        for (const scale of [1,2]) {
            const pages = {};
            for (const variant of ['original','owned']) {
                pages[variant] = await load(browser,variant,scale);
                await pages[variant].page.evaluate(() => {
                    window.rangeStep = function(step) {
                        if (step === 0) {
                            basics.setVisibleRangeX(1000.25, 8000.75);
                            basics.range_y = {from:spec.scope ? -.3 : spec.logY ? .1 : -140, to:spec.scope ? .3 : spec.logY ? 10 : -90};
                        } else if (step === 1) {
                            basics.setVisibleRangeX(4090.5, 4100.5); basics.range_y = {};
                        } else if (step === 2) {
                            basics.setVisibleRangeX(1,spec.bins); basics.range_y = {};
                        } else if (step === 3) {
                            basics.setPrimaryTraceLabel('Updated');
                            extras.length = 0;
                        } else if (step === 4) {
                            basics.setVisibleRangeX(4090.5, 4100.5);
                            basics.range_y = {from:spec.scope ? -.1 : spec.logY ? .2 : -130.1, to:spec.scope ? .1 : spec.logY ? .4 : -129.9};
                        } else if (step === 5) {
                            basics.setRangeX(1,spec.bins); basics.setLinY();
                        } else if (step === 6) {
                            basics.setLogY(); basics.LogYaxisFormatter = v => String(v);
                        } else if (step === 7) {
                            basics.LogYaxisFormatter = v => 'value ' + String(v);
                        }
                        basics.reset_range = true;
                        render(step);
                        const p = basics.plot, axes = p.getAxes();
                        return {offset:p.getPlotOffset(), size:[p.getCanvas().width,p.getCanvas().height],
                            axes:Object.fromEntries(Object.entries(axes).map(([key,a]) => [key,{min:a.min,max:a.max,
                                datamin:a.datamin,datamax:a.datamax,ticks:a.ticks, labels:[a.labelWidth,a.labelHeight],
                                configuredBounds:[a.options.min,a.options.max]}])),
                            series:p.getData().map(s => ({data:s.data,points:s.datapoints.points,color:s.color,width:s.lines.lineWidth})),
                            legend:p.getPlaceholder().find('.legend').html(), image:p.getCanvas().toDataURL()};
                    };
                });
            }
            for (let scenario = 0; scenario < 5; scenario++) {
                for (const {page} of Object.values(pages)) await page.evaluate(i => {
                    setup(i);
                    // PNA/DPLL signed estimates use point markers alongside
                    // ordinary lines, including during narrow range updates.
                    if(i===1) {extras[0].points={show:true,radius:2,lineWidth:0}; extras[0].lines={show:false};}
                },scenario);
                for (let step = 0; step < 8; step++) {
                    const original = await pages.original.page.evaluate(step => rangeStep(step),step);
                    const owned = await pages.owned.page.evaluate(step => rangeStep(step),step);
                    if (original.image !== owned.image) {
                        const fraction = await pages.owned.page.evaluate(async urls => {
                            const imgs = await Promise.all(urls.map(url => new Promise(resolve => {
                                const img = new Image(); img.onload=()=>resolve(img); img.src=url;
                            })));
                            const canvas = document.createElement('canvas'); canvas.width=imgs[0].width; canvas.height=imgs[0].height;
                            const ctx = canvas.getContext('2d');
                            ctx.drawImage(imgs[0],0,0); const before=ctx.getImageData(0,0,canvas.width,canvas.height).data;
                            ctx.clearRect(0,0,canvas.width,canvas.height); ctx.drawImage(imgs[1],0,0);
                            const after=ctx.getImageData(0,0,canvas.width,canvas.height).data;
                            let different=0;
                            for(let i=0;i<before.length;i+=4) if([0,1,2,3].some(c=>Math.abs(before[i+c]-after[i+c])>8)) different++;
                            return different/(before.length/4);
                        },[original.image,owned.image]);
                        assert.ok(fraction<.001,`range image drift ${fraction}, scenario ${scenario}, step ${step}, DPR ${scale}`);
                    }
                    delete original.image; delete owned.image;
                    assert.deepEqual(owned,original,`ranges: scenario ${scenario}, step ${step}, DPR ${scale}`);
                }
            }
            for (const {page,errors} of Object.values(pages)) {assert.deepEqual(errors,[]); await page.close();}
        }
        const {page,errors} = await load(browser,'owned',2);
        const reuse = await page.evaluate(() => {
            setup(0);
            const p = basics.plot, buffer = p.getData()[0].datapoints.points;
            p.setSelection({xaxis:{from:1000,to:8000},yaxis:{from:-140,to:-90}},true);
            basics.setVisibleRangeX(1000,8000); render();
            const result = {samePlot:basics.plot===p, sameBuffer:basics.plot.getData()[0].datapoints.points===buffer,
                selection:basics.plot.getSelection()};
            basics.setLogX(true); basics.reset_range=true; render();
            result.modeRebuilt = basics.plot!==p;
            return result;
        });
        assert.deepEqual(reuse,{samePlot:true,sameBuffer:true,selection:null,modeRebuilt:true});
        await page.evaluate(() => {setup(0); window.beforeDrag=basics.plot;});
        await page.mouse.move(200,180); await page.mouse.down(); await page.mouse.move(400,280);
        const activeDrag = await page.evaluate(() => {
            const wasActive = basics.plot.isSelectionActive();
            basics.setVisibleRangeX(1000,8000); render();
            return {wasActive, rebuilt:beforeDrag!==basics.plot, active:basics.plot.isSelectionActive()};
        });
        await page.mouse.up();
        assert.deepEqual(activeDrag,{wasActive:true,rebuilt:true,active:false},'range changes during a drag retain cancellation behavior');
        const selectionCounts = await page.evaluate(() => {
            const el = basics.plot.getPlaceholder(); window.selections=0;
            el.on('plotselected.regression',()=>selections++);
            for(let i=0;i<50;i++) {basics.setVisibleRangeX(1000+i,8000-i);render();}
            basics.plot.setSelection({xaxis:{from:2000,to:4000},yaxis:{from:-130,to:-100}});
            return selections;
        });
        assert.equal(selectionCounts,1,'repeated range updates do not duplicate selection handlers');
        const fallbacks = await page.evaluate(() => {
            setup(0); extras[0].yaxis=2; basics.reset_range=true;render();
            const multi = basics.plot;
            basics.setVisibleRangeX(1000,8000);render();
            const multiAxis = {rebuilt:basics.plot!==multi, axes:basics.plot.getYAxes().length};
            setup(0); basics.options.xaxis.axisLabel='Frequency'; basics.rebuildPlot=true;basics.reset_range=true;render();
            const labeled = basics.plot;
            basics.setVisibleRangeX(1000,8000);render();
            return {multiAxis,label:{rebuilt:basics.plot!==labeled, text:basics.plot.getAxes().xaxis.options.axisLabel}};
        });
        assert.deepEqual(fallbacks,{multiAxis:{rebuilt:true,axes:2},label:{rebuilt:true,text:'Frequency'}},'multiple axes and axis labels retain full plugin initialization');
        const cdp = await page.context().newCDPSession(page);
        await page.evaluate(()=>{setup(0);window.beforeDPR=basics.plot;});
        await cdp.send('Emulation.setDeviceMetricsOverride',{width:1200,height:800,deviceScaleFactor:1,mobile:false});
        const density = await page.evaluate(()=>{
            const needsRedraw=basics.needsRedraw();render();
            return {needsRedraw,rebuilt:basics.plot!==beforeDPR,width:basics.plot.getCanvas().width,dpr:devicePixelRatio};
        });
        assert.deepEqual(density,{needsRedraw:true,rebuilt:true,width:1000,dpr:1},'pixel-density changes refresh the canvas and backing resolution');
        await cdp.send('Emulation.setDeviceMetricsOverride',{width:1200,height:800,deviceScaleFactor:2,mobile:false});
        const highDPI = await page.evaluate(()=>{
            basics.setVisibleRangeX(2000,7000);render();
            return {width:basics.plot.getCanvas().width,dpr:devicePixelRatio};
        });
        assert.deepEqual(highDPI,{width:2000,dpr:2},'zoom/reset adopts a new display density');
        assert.deepEqual(errors,[]); await page.close();
        console.log('Repeated ranges, autoscale/ticks/legend, buffer reuse, mode changes and active-drag fallback: PASS');
    } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
