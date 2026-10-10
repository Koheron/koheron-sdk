// Read-only verification of the deployed FFT management UI; never issues mutations.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const puppeteer = require(process.env.PUPPETEER_MODULE || 'puppeteer-core');

(async () => {
  const output = process.env.BROWSER_OUTPUT || 'tmp/management-readonly', base = process.env.MANAGEMENT_URL;
  assert.ok(base, 'Set MANAGEMENT_URL to the deployed /koheron/ URL');
  assert.ok(process.env.CHROMIUM_PATH, 'Set CHROMIUM_PATH to Chromium');
  fs.mkdirSync(output, {recursive: true});
  const checks = [], errors = [], failures = [], viewports = [], websocketFrames = [];
  let statusReads = 0, instrumentFrames = 0;
  const instrumentSockets = new Map();
  const passed = name => { checks.push(name); console.log('PASS: ' + name); };
  const browser = await puppeteer.launch({executablePath: process.env.CHROMIUM_PATH, headless: true, args: ['--no-sandbox']});
  const page = await browser.newPage();
  page.on('pageerror', error => errors.push(error.message));
  page.on('requestfailed', request => failures.push({url: request.url(), error: request.failure().errorText}));
  page.on('request', request => {
    assert.equal(request.method(), 'GET', 'Production verification must only issue GET requests');
    if (request.url().endsWith('/api/system/status')) ++statusReads;
  });
  const cdp = await page.createCDPSession();
  await cdp.send('Network.enable');
  cdp.on('Network.webSocketCreated', event => {
    const socketUrl = new URL(event.url);
    if (socketUrl.hostname === new URL(base).hostname && socketUrl.port === '8080') instrumentSockets.set(event.requestId, event.url);
  });
  cdp.on('Network.webSocketFrameReceived', event => {
    if (instrumentSockets.has(event.requestId)) ++instrumentFrames;
    try { const value = JSON.parse(event.response.payloadData); if (value.type === 'status') websocketFrames.push({instrument: value.current_instrument?.name, phase: value.operation.phase}); } catch {}
  });
  try {
    await page.setViewport({width: 1360, height: 900});
    await page.goto(base, {waitUntil: 'networkidle0'});
    await page.waitForFunction(() => document.querySelector('#board-connection').textContent === 'Connected');
    assert.equal(await page.$eval('#live-label', node => node.textContent), 'Running');
    assert.equal(await page.$eval('#live-name', node => node.textContent), 'fft');
    assert.equal(await page.$eval('#live-version', node => node.textContent), 'v0.3.0');
    assert.equal(await page.$eval('#instrument-stop', node => node.hidden), false);
    passed('current management page identifies the real running FFT and exposes lifecycle controls');
    await page.waitForFunction(() => document.querySelector('#health-table').rows.length > 0 && document.querySelector('#release-table').rows.length > 0);
    await page.waitForFunction(() => document.querySelector('#koheron-log').textContent.length > 0);
    passed('real board health, operating-system information and instrument journal render');
    const initialReads = statusReads, initialFrames = websocketFrames.length;
    await new Promise(resolve => setTimeout(resolve, 12000));
    assert.equal(statusReads, initialReads);
    assert.ok(websocketFrames.length > initialFrames);
    assert.ok(websocketFrames.every(frame => frame.instrument === 'fft' && frame.phase === 'idle'));
    assert.equal(await page.$eval('#board-connection', node => node.textContent), 'Connected');
    passed('real port-80 WebSocket remains connected for 12 seconds without HTTP fallback');
    await page.screenshot({path: path.join(output, 'desktop.png'), fullPage: true});
    await page.click('tr[data-name="fft"] summary');
    await page.click('tr[data-name="fft"] .instrument-menu button');
    await page.waitForFunction(() => document.querySelector('#preflight-content').textContent.includes('Ready to run'));
    assert.equal(await page.evaluate(() => document.activeElement.id), 'preflight-close');
    await page.click('#preflight-close');
    assert.equal(await page.evaluate(() => document.activeElement.getAttribute('aria-label')), 'More actions for fft');
    passed('production FFT compatibility check and keyboard focus round trip');
    for (const width of [320, 390, 768, 1360]) {
      await page.setViewport({width, height: 844});
      const geometry = await page.evaluate(() => ({width: innerWidth, scrollWidth: document.documentElement.scrollWidth}));
      assert.ok(geometry.scrollWidth <= width, JSON.stringify(geometry)); viewports.push(geometry);
      if (width === 390) await page.screenshot({path: path.join(output, 'mobile.png'), fullPage: true});
    }
    passed('management layouts fit 320, 390, 768 and 1360 pixel screens');
    await page.setViewport({width: 1360, height: 900});
    await page.goto(new URL('instrument_summary.html?name=fft', base).href, {waitUntil: 'networkidle0'});
    await page.waitForSelector('.command-group');
    await page.waitForFunction(() => document.querySelector('#instrument-check').textContent.includes('Ready to run'));
    assert.equal(await page.$eval('#instrument-name', node => node.textContent), 'fft');
    await page.screenshot({path: path.join(output, 'details.png'), fullPage: true});
    passed('production instrument details, commands and compatibility render');
    await page.goto(new URL('logs_rate.html', base).href, {waitUntil: 'networkidle0'});
    await page.waitForFunction(() => document.querySelector('#logs-rate-status').textContent === 'Live');
    assert.notEqual(await page.$eval('#rate-sessions', node => node.textContent), '—');
    await page.screenshot({path: path.join(output, 'rates.png'), fullPage: true});
    passed('production transfer-rate page receives live server statistics');
    await page.goto(new URL('/', base).href, {waitUntil: 'domcontentloaded'});
    await page.waitForFunction(() => document.body.innerText.includes('Live spectrum') &&
      /[1-9]\d* FPS/.test(document.body.innerText) && document.body.innerText.includes('2,048 points'));
    assert.ok(instrumentSockets.size > 0 && instrumentFrames > 0);
    await page.screenshot({path: path.join(output, 'fft.png'), fullPage: true});
    passed('real FFT page receives instrument WebSocket frames and renders a live 2048-point spectrum');
    assert.deepEqual(errors, []); assert.deepEqual(failures, []);
    passed('no browser JavaScript errors or failed resource requests');
  } finally {
    fs.writeFileSync(path.join(output, 'browser-results.json'), JSON.stringify({recorded_utc: new Date().toISOString(), url: base, scope: 'Read-only production verification: GET requests and FFT reads; no lifecycle, upload, default, removal or acquisition-setting actions', checks, viewports, statusReads, websocketFrames, instrumentSocketUrls: [...instrumentSockets.values()], instrumentFrames, errors, failures}, null, 2) + '\n');
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
