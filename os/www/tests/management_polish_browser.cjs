// Run against the isolated board preview, never the production instrument.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const puppeteer = require(process.env.PUPPETEER_MODULE || 'puppeteer-core');

(async () => {
    const url = process.env.MANAGEMENT_URL;
    assert.ok(url, 'Set MANAGEMENT_URL to the private preview /koheron/ URL');
    const output = process.env.BROWSER_OUTPUT || 'tmp/management-polish';
    fs.mkdirSync(output, {recursive: true});
    const browser = await puppeteer.launch({executablePath: process.env.CHROMIUM_PATH, headless: true, args: ['--no-sandbox']});
    const page = await browser.newPage();
    const errors = [], checks = [], viewports = [];
    let statusReads = 0, failStatus = false, failCommands = false;
    const passed = name => { checks.push(name); console.log('PASS: ' + name); };
    page.on('pageerror', error => errors.push(error.message));
    page.on('request', request => { if (request.url().endsWith('/api/system/status')) ++statusReads; });
    await page.setRequestInterception(true);
    page.on('request', request => {
        const blocked = (failStatus && request.url().endsWith('/api/system/status')) ||
            (failCommands && request.url().includes('/api/instruments/commands/'));
        void (blocked ? request.abort() : request.continue());
    });
    const connected = () => page.waitForFunction(() => document.querySelector('#board-connection').textContent === 'Connected');
    const geometry = () => page.evaluate(() => ({width: innerWidth, scrollWidth: document.documentElement.scrollWidth,
        tableWidth: document.querySelector('#instruments-table')?.getBoundingClientRect().width,
        tableContainerWidth: document.querySelector('.table-scroll')?.clientWidth}));
    try {
        await page.setViewport({width: 1360, height: 900});
        await page.goto(url, {waitUntil: 'networkidle0'}); await connected();
        await page.click('#instrument-stop');
        await page.waitForFunction(() => document.querySelector('#live-label').textContent === 'Stopped' &&
            document.querySelector('#runtime-operation').textContent === 'Instrument stopped.');
        await page.click('#instrument-start');
        await page.waitForFunction(() => document.querySelector('#live-label').textContent === 'Running' &&
            document.querySelector('#runtime-operation').textContent === 'Instrument started.');
        passed('real board Stop/Start with readable completed feedback');
        const before = statusReads;
        await new Promise(resolve => setTimeout(resolve, 12000));
        assert.equal(statusReads, before); await connected();
        passed('WebSocket heartbeat remains connected without HTTP fallback');
        await page.screenshot({path: path.join(output, 'desktop.png'), fullPage: true});

        await page.click('tr[data-name="scope"] summary');
        await page.click('tr[data-name="old"] summary');
        assert.equal(await page.$$eval('.instrument-options[open]', menus => menus.length), 1);
        await page.keyboard.press('Escape');
        assert.equal(await page.$$eval('.instrument-options[open]', menus => menus.length), 0);
        assert.equal(await page.evaluate(() => document.activeElement.getAttribute('aria-label')), 'More actions for old');
        await page.click('tr[data-name="scope"] summary'); await page.click('#instruments-heading');
        assert.equal(await page.$$eval('.instrument-options[open]', menus => menus.length), 0);
        passed('one action menu at a time; outside click and Escape restore focus');

        await page.click('tr[data-name="scope"] summary');
        await page.click('tr[data-name="scope"] .instrument-menu button');
        await page.waitForFunction(() => document.querySelector('#preflight-content').textContent.includes('Ready to run'));
        assert.equal(await page.evaluate(() => document.activeElement.id), 'preflight-close');
        await page.screenshot({path: path.join(output, 'preflight.png'), fullPage: true});
        await page.click('#preflight-close');
        assert.equal(await page.evaluate(() => document.activeElement.getAttribute('aria-label')), 'More actions for scope');
        passed('real preflight and keyboard focus round trip');

        for (const width of [320, 390, 768, 1360]) {
            await page.setViewport({width, height: 844});
            const value = await geometry(); assert.ok(value.scrollWidth <= width, JSON.stringify(value)); viewports.push(value);
            if (width === 390) {
                assert.ok(await page.$eval('#upload-btn', button => button.getBoundingClientRect().height >= 36));
                await page.screenshot({path: path.join(output, 'mobile.png'), fullPage: true});
                await page.click('tr[data-name="scope"] summary');
                assert.ok(await page.$eval('tr[data-name="scope"] .instrument-menu button', button => button.getBoundingClientRect().height >= 44));
                const menu = await geometry(); assert.ok(menu.scrollWidth <= width); viewports.push({...menu, menuOpen: true});
                await page.screenshot({path: path.join(output, 'mobile-menu.png'), fullPage: true});
                await page.keyboard.press('Escape');
            }
        }
        passed('320/390/768/1360px layouts and mobile touch targets');

        failStatus = true;
        await page.evaluate(() => runtime.socket.close());
        await page.waitForFunction(() => document.querySelector('#board-connection').textContent === 'Disconnected');
        assert.equal(await page.$eval('#refresh-instruments', button => button.disabled), false);
        assert.equal(await page.$eval('#upload-btn', button => button.disabled), true);
        failStatus = false; await page.click('#refresh-instruments'); await connected();
        assert.equal(await page.$eval('#upload-btn', button => button.disabled), false);
        passed('simulated stream loss and failed status read retain a working manual Retry');

        failCommands = true;
        await page.goto(new URL('instrument_summary.html?name=scope', url).href, {waitUntil: 'networkidle0'});
        await page.waitForSelector('#instrument-commands-retry:not([hidden])');
        failCommands = false; await page.click('#instrument-commands-retry');
        await page.waitForSelector('.command-group');
        await page.waitForFunction(() => document.querySelector('#instrument-check').textContent.includes('Ready to run'));
        await page.click('#instrument-check-refresh');
        await page.waitForFunction(() => !document.querySelector('#instrument-check-refresh').disabled);
        await page.screenshot({path: path.join(output, 'details.png'), fullPage: true});
        passed('simulated command request failure retries against the real API; compatibility check repeats');

        await page.goto(new URL('instrument_summary.html', url).href, {waitUntil: 'networkidle0'});
        assert.equal(await page.$eval('#instrument-name', node => node.textContent), 'No instrument selected');
        assert.equal(await page.$eval('#instrument-check-heading', node => node.closest('section').hidden), true);
        passed('missing instrument name has a clear return path without loading placeholders');
        assert.deepEqual(errors, []); passed('no Chromium page errors');
        fs.writeFileSync(path.join(output, 'browser.json'), JSON.stringify({url, checks, viewports, statusReads, errors}, null, 2) + '\n');
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
