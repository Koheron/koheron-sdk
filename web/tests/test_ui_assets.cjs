// Exercise the shipped jQuery/Bootstrap pairing in a real browser.
const assert = require('node:assert/strict');
const path = require('node:path');
const {test} = require('node:test');
const {launch, root} = require('../plotting/benchmark/harness.cjs');

test('OS and instrument UI assets support modal, dropdown, tabs and sanitized tooltips', async () => {
    const browser = await launch();
    try {
        for (const directory of ['tmp/www', 'tmp/examples/alpha250/fft/web']) {
            const page = await browser.newPage();
            const errors = [];
            page.on('pageerror', error => errors.push(error.message));
            await page.setContent(`<!doctype html><html><body>
                <div id="dialog" class="modal"><div class="modal-dialog"><div class="modal-content">Dialog</div></div></div>
                <div class="dropdown"><button id="menu" data-toggle="dropdown">Menu</button><ul class="dropdown-menu"><li>Item</li></ul></div>
                <ul class="nav nav-tabs"><li class="active"><a href="#first" data-toggle="tab">First</a></li><li><a id="tab" href="#second" data-toggle="tab">Second</a></li></ul>
                <div class="tab-content"><div id="first" class="tab-pane active">First</div><div id="second" class="tab-pane">Second</div></div>
                <button id="tip">Tooltip</button>
                </body></html>`);
            await page.addStyleTag({path: path.join(root, directory, 'bootstrap.min.css')});
            for (const filename of ['jquery.min.js', 'bootstrap.min.js']) {
                await page.addScriptTag({path: path.join(root, directory, filename)});
            }
            const result = await page.evaluate(() => {
                $.support.transition = false;
                $('#dialog').modal('show');
                const modal = $('#dialog').hasClass('in') && $('body').hasClass('modal-open');
                $('#dialog').modal('hide');
                const closed = !$('#dialog').hasClass('in') && !$('.modal-backdrop').length;
                $('#menu').trigger('click');
                const dropdown = $('.dropdown').hasClass('open');
                $('#tab').tab('show');
                const tab = $('#second').hasClass('active') && !$('#first').hasClass('active');
                $('#tip').tooltip({html: true, title: '<b>Safe</b><img src="x" onerror="window.injected=true">'}).tooltip('show');
                const sanitized = $('.tooltip b').text() === 'Safe' && !$('.tooltip [onerror]').length && !window.injected;
                return {jquery: $.fn.jquery, bootstrap: $.fn.modal.Constructor.VERSION, modal, closed, dropdown, tab, sanitized};
            });
            assert.deepEqual(result, {jquery: '3.7.1', bootstrap: '3.4.1', modal: true, closed: true, dropdown: true, tab: true, sanitized: true}, directory);
            assert.deepEqual(errors, [], directory);
            await page.close();
        }
    } finally {
        await browser.close();
    }
});

test('generated dashboard and instrument scripts load with their global entry points', async () => {
    const browser = await launch();
    try {
        const scripts = [
            ['tmp/www/instruments.js', 'typeof Client === "function" && typeof Imports === "function" && typeof InstrumentsWidget === "function"'],
            ['tmp/examples/alpha250/fft/web/app.js', 'app instanceof FFTWorkspace && typeof FFT === "function"'],
            ['tmp/examples/alpha15/signal-analyzer/web/app.js', 'app instanceof FFTWorkspace && typeof Alpha15SignalAnalyzerControls === "function"'],
            ['tmp/examples/alpha250-4/phase-noise-analyzer/web/app.js', 'app instanceof App && typeof Plot === "function"'],
        ];
        for (const [script, check] of scripts) {
            const page = await browser.newPage();
            const errors = [];
            page.on('pageerror', error => errors.push(error.message));
            await page.setContent('<!doctype html><html><body><div id="plot-placeholder"></div></body></html>');
            await page.addScriptTag({path: path.join(root, 'tmp/www/jquery.min.js')});
            await page.addScriptTag({path: path.join(root, script)});
            assert.equal(await page.evaluate(check), true, script);
            assert.deepEqual(errors, [], script);
            await page.close();
        }
    } finally {
        await browser.close();
    }
});
