const assert = require('node:assert/strict');
const {launch,load} = require('../benchmark/harness.cjs');
const {previewHTML} = require('../benchmark/persistence-preview.cjs');
(async () => {
    const browser = await launch();
    try {
        for (const scale of [1,2]) {
            const {page,errors} = await load(browser,'owned',scale);
            const result = await page.evaluate(() => {
                setup(0);
                let now = 0;
                Object.defineProperty(performance,'now',{configurable:true,value:() => now});
                const old = Array.from({length:1024},(_,i) => [i+1,-110]);
                const current = old.map(([x]) => [x,-145]);
                const ref = old.map(([x]) => [x,-95]);
                const original = JSON.stringify([old,current,ref]);
                basics.range_x = {from:1,to:1024}; basics.range_y = {from:-180,to:-65};
                basics.setPersistence(true,1,.4);
                const draw = (data,frame) => basics.redraw(data,data.length,[],'Power',() => {},ref,true,[],undefined,frame);
                const alpha = y => {
                    const p = basics.plot.pointOffset({x:512,y});
                    const canvas = basics.persistenceLayer;
                    const ratio = canvas.width / basics.plot.getPlaceholder().width();
                    const bytes = canvas.getContext('2d').getImageData(Math.floor(p.left*ratio)-2,Math.floor(p.top*ratio)-2,5,5).data;
                    return Math.max(...Array.from(bytes).filter((_,i) => i%4===3))*Math.exp(-basics.persistenceFade);
                };
                const pixels = () => basics.persistenceLayer.getContext('2d').getImageData(0,0,basics.persistenceLayer.width,basics.persistenceLayer.height).data;
                draw(old,0); const initial = alpha(-110);
                now=100; draw(current,1);
                const trail = alpha(-110), reference = alpha(-95);
                now=1100; draw(current,1); const faded = alpha(-110);
                const beforePause = pixels();
                const unchanged = () => pixels().every((value,i) => value===beforePause[i]);
                basics.setPersistencePaused(true);
                now=11100; draw(current,1); const frozen = unchanged();
                now=12100; basics.setPersistencePaused(false); draw(current,1);
                const resume = unchanged();
                now=13100; draw(current,1); const resumedFade = alpha(-110);
                const retainedLayer=basics.persistenceLayer;
                basics.setPersistence(true,1,.3);
                const settingsRetain=basics.persistenceLayer===retainedLayer;
                // A complete trace is composited once, without restamping it on
                // cursor/reference redraws of the same received frame.
                const currentBefore = alpha(-145); draw(current,1); const sameFrame = alpha(-145);
                const buffers = [basics.persistenceLayer,basics.persistenceTrace];
                const size = buffers.map(c => [c.width,c.height]);
                basics.clearPersistence(); draw(current,1); const cleared = alpha(-110);
                basics.setVisibleRangeX(100,900); draw(current,1); const zoomed = alpha(-110);
                basics.setLinY(); draw(current,1); const units = alpha(-110);
                const ph = basics.plot.getPlaceholder(); ph.width(700); ph.height(350); basics.needsRedraw(); draw(current,1);
                const resized = [basics.persistenceLayer.width,basics.persistenceLayer.height];
                basics.setPersistence(true,5,.15); draw(old,2);
                for (let i=0;i<800;i++) { now+=50; draw(current,3); }
                const slowCleared = alpha(-110);
                basics.setPersistence(false); draw(current,1);
                const disabled = !basics.persistenceLayer && basics.plot.hooks.drawSeries.length===0;
                // Context restoration remains correct even if custom point
                // rendering throws while drawing into the detached layer.
                const p = basics.plot, s = p.getData()[0];
                const trace = document.createElement('canvas').getContext('2d');
                s.lines.show=false; s.points.show=true; s.points.symbol=() => {throw new Error('test symbol');};
                let caught=false; try {p.drawSeriesToCanvas(s,trace);} catch(e) {caught=e.message==='test symbol';}
                s.points.show=false; s.lines.show=true; p.draw();
                const normalCanvas = p.getCanvas().getContext('2d').getImageData(0,0,p.getCanvas().width,p.getCanvas().height).data;
                const restored = normalCanvas.some((v,i) => i%4===3 && v>0);
                p.shutdown(); delete performance.now;
                return {initial,trail,reference,faded,frozen,resume,resumedFade,settingsRetain,currentBefore,sameFrame,size,cleared,zoomed,units,resized,slowCleared,disabled,caught,restored,
                    released:buffers.every(c => c.width===0 && c.height===0),sourceUnchanged:original===JSON.stringify([old,current,ref])};
            });
            assert.ok(result.initial>80 && result.trail>60,'previous live trace remains visible');
            assert.equal(result.reference,0,'references are excluded from the persistence buffers');
            assert.ok(Math.abs(result.faded/result.trail-Math.exp(-1))<.04,'decay follows elapsed time');
            assert.ok(result.frozen && result.resume,'pause freezes trails and resume excludes paused time');
            assert.ok(result.resumedFade<result.faded,'decay resumes');
            assert.equal(result.sameFrame,result.currentBefore,'same frame does not increase trail intensity');
            assert.ok(result.settingsRetain,'decay/intensity edits retain existing trails');
            assert.deepEqual(result.size,[[1000*scale,500*scale],[1000*scale,500*scale]]);
            assert.equal(result.cleared,0); assert.equal(result.zoomed,0); assert.equal(result.units,0);
            assert.deepEqual(result.resized,[700*scale,350*scale]);
            assert.equal(result.slowCleared,0,'small decay steps do not leave stuck 8-bit ghost pixels');
            assert.ok(result.disabled && result.released && result.sourceUnchanged);
            assert.ok(result.caught && result.restored,'detached rendering restores the live context after exceptions');
            assert.deepEqual(errors,[]); await page.close();
            console.log(`Persistence trails, elapsed decay, pause, resets, DPR ${scale}, buffers and source preservation: PASS`);
        }
        const page = await browser.newPage({viewport:{width:1200,height:850},deviceScaleFactor:2});
        const errors=[]; page.on('pageerror',e=>errors.push(e.message));
        await page.setContent(previewHTML());
        await page.waitForFunction(()=>window.spectrum?.plot_data.length && basics.persistenceLayer);
        await page.locator('#pause-display').click();
        assert.equal(await page.locator('#pause-display').textContent(),'Resume');
        await page.locator('#persistence-enabled').uncheck();
        assert.equal(await page.locator('#persistence-controls').isVisible(),false);
        assert.equal(await page.evaluate(()=>!basics.persistenceLayer),true);
        await page.locator('#persistence-enabled').check();
        await page.locator('#persistence-decay').selectOption('2');
        await page.locator('#persistence-intensity').fill('30');
        assert.equal(await page.locator('#persistence-intensity-value').textContent(),'30%');
        await page.locator('#capture-reference').click();
        await page.locator('#pause-display').click();
        await page.waitForFunction(()=>spectrum.history.samples>20);
        await page.locator('#pause-display').click();
        await page.screenshot({path:'tmp/plotting/screenshots/persistence-preview.png'});
        const exports = await page.evaluate(async () => {
            const source=JSON.stringify(spectrum.plot_data);
            ExportFile.prototype.download=function(blob){window.exportedBlob=blob;};
            document.querySelector('.export-data').click(); const csv=await exportedBlob.text();
            exportedBlob=undefined; document.querySelector('.export-plot').click();
            while (!exportedBlob) {await new Promise(resolve=>setTimeout(resolve,1));}
            const bitmap=await createImageBitmap(exportedBlob), base=basics.getExportCanvas();
            const scale=base.width/basics.plot.getCanvas().clientWidth, top=bitmap.height-base.height-30*scale;
            const expected=document.createElement('canvas');expected.width=base.width;expected.height=base.height;
            const ctx=expected.getContext('2d');ctx.fillStyle='white';ctx.fillRect(0,0,base.width,base.height);ctx.drawImage(base,0,0);
            const before=ctx.getImageData(0,0,base.width,base.height).data;
            ctx.clearRect(0,0,base.width,base.height);ctx.drawImage(bitmap,0,-top);
            const after=ctx.getImageData(0,0,base.width,base.height).data;
            bitmap.close();
            return {png:before.every((v,i)=>Math.abs(v-after[i])<=1),csv:csv.includes(spectrum.plot_data.at(-1).join(',')),
                source:source===JSON.stringify(spectrum.plot_data),label:spectrum.persistenceLabel};
        });
        assert.ok(exports.png && exports.csv && exports.source,'actual PNG preserves trails at DPR 2 and CSV retains raw samples');
        assert.equal(exports.label,'Persistence · 2 s decay · 30% intensity');
        const box=await page.locator('#plot-placeholder .flot-overlay').boundingBox();
        await page.mouse.move(box.x+box.width*.25,box.y+box.height*.35);
        await page.mouse.down();
        await page.mouse.move(box.x+box.width*.75,box.y+box.height*.7,{steps:8});
        await page.mouse.up();
        assert.equal(await page.evaluate(()=>basics.getRangeX().to<125),true,'trusted drag still zooms above the persistence layer');
        await page.locator('#clear-persistence').click();
        assert.equal(await page.evaluate(()=>basics.persistenceLayer.getContext('2d').getImageData(0,0,basics.persistenceLayer.width,basics.persistenceLayer.height).data.every(v=>v===0)),true);
        await page.locator('#spectrum-view').selectOption('density');
        assert.equal(await page.locator('#trace-controls').isVisible(),false);
        await page.locator('#spectrum-view').selectOption('spectrum');
        assert.equal(await page.locator('#persistence-controls').isVisible(),true);
        await page.setViewportSize({width:390,height:844});
        await page.waitForFunction(()=>basics.persistenceLayer?.width===basics.plot.getCanvas().width);
        assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,'controls fit a narrow viewport');
        await page.screenshot({path:'tmp/plotting/screenshots/persistence-mobile.png'});
        await page.evaluate(()=>spectrum.dispose());
        assert.equal(await page.locator('#persistence-enabled').isDisabled(),true);
        assert.equal(await page.evaluate(()=>!basics.persistenceLayer),true);
        assert.deepEqual(errors,[]); await page.close();
        console.log('Actual FFT persistence controls, references, history switching, CSV/PNG exports and disposal: PASS');
    } finally {await browser.close();}
})().catch(e => {console.error(e);process.exitCode=1;});
