// Hook payloads, receiver and live-list edits must match the original Flot.
const assert = require('node:assert/strict');
const {launch, load} = require('../benchmark/harness.cjs');
(async () => {
    const browser = await launch();
    try {
        const results = [];
        for (const variant of ['original', 'owned']) {
            const {page, errors} = await load(browser, variant);
            results.push(await page.evaluate(async () => {
                const el = $('<div style="width:400px;height:200px">').appendTo('body');
                const events = [], hooks = {}, input = [{label:'A',data:[[1,2],[2,3]]}, {label:'B',data:[[1,4],[2,5]]}];
                let operation = 'create', edited = false, nested = false;
                const check = (name, args) => {
                    const [p,a,b,c] = args;
                    switch (name) {
                    case 'processOptions': return args.length === 2 && a === p.getOptions();
                    case 'processRawData': return args.length === 4 && p.getData().includes(a) && b === a.data && c === a.datapoints;
                    case 'processDatapoints': return args.length === 3 && p.getData().includes(a) && b === a.datapoints;
                    case 'processOffset': return args.length === 2 && a === p.getPlotOffset();
                    case 'drawSeries': return args.length === 3 && a === p.getCanvas().getContext('2d') && p.getData().includes(b);
                    case 'drawBackground': case 'draw': return args.length === 2 && a === p.getCanvas().getContext('2d');
                    case 'drawOverlay': return args.length === 2 && a === p.getPlaceholder().find('.flot-overlay')[0].getContext('2d');
                    case 'bindEvents': case 'shutdown': return args.length === 2 && a[0] === p.getPlaceholder().find('.flot-overlay')[0];
                    }
                };
                const names = ['processOptions','processRawData','processDatapoints','processOffset',
                    'drawBackground','drawSeries','draw','drawOverlay','bindEvents','shutdown'];
                for (const name of names) hooks[name] = [function() {
                    events.push([operation,name,arguments.length,this === window,check(name,Array.from(arguments))]);
                }];
                let firstOverlay;
                const overlayDrawn = new Promise(resolve => {firstOverlay = resolve;});
                hooks.drawOverlay.push(firstOverlay);
                hooks.processRawData.push(function(p,s) {
                    events.push([operation,'edit',arguments.length,this === window,s.data === input[p.getData().indexOf(s)].data]);
                    if (operation === 'data' && !nested) {
                        nested = true; operation = 'nested';
                        p.triggerRedrawOverlay(); operation = 'data';
                    }
                    if (!edited) {
                        edited = true;
                        p.hooks.processRawData.splice(p.hooks.processRawData.indexOf(removed),1);
                        p.hooks.processRawData.push(function(p,s,data,dp) {
                            events.push([operation,'added',arguments.length,this === window,data === s.data && dp === s.datapoints]);
                        });
                    }
                });
                function removed() {events.push([operation,'removed']);}
                hooks.processRawData.push(removed);
                const plot = $.plot(el,input,{series:{reuseDatapoints:true,shadowSize:0},hooks});
                await overlayDrawn;
                plot.getOptions().interaction.redrawOverlayInterval = -1;
                operation = 'data'; plot.setData(input);
                operation = 'grid'; plot.setupGrid();
                operation = 'draw'; plot.draw();
                operation = 'overlay'; plot.highlight(0,0); plot.unhighlight(); plot.triggerRedrawOverlay();
                operation = 'shutdown'; plot.shutdown(); el.remove();
                return {events, inputHookCount:hooks.processRawData.length};
            }));
            assert.deepEqual(errors, []); await page.close();
        }
        assert.deepEqual(results[1],results[0],'owned hooks preserve original callback order, payload identity, arity and receiver');
        assert.equal(results[1].inputHookCount,3,'editing plot hooks leaves caller hook arrays intact');
        assert.ok(results[1].events.every(event => event.length === 5 && event[3] && event[4]),'all hook payload/receiver checks pass; removed listener is skipped');
        assert.equal(results[1].events.filter(event => event[1] === 'added').length,4,'appended listener runs in the current dispatch and later dispatches');
        assert.equal(results[1].events.filter(event => event[0] === 'nested' && event[1] === 'drawOverlay').length,1,'nested dispatch preserves the outer payload');
        assert.equal(new Set(results[1].events.filter(event => event[1] !== 'edit' && event[1] !== 'added').map(event => event[1])).size,10,'every hook stage is exercised');
        console.log('Hook arguments/arity, receiver, callback order and live-list edits: PASS');
    } finally {await browser.close();}
})().catch(e => {console.error(e);process.exitCode=1;});
