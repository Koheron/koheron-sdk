// Reproduce an older cached bundle, then verify content versions bypass it.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const puppeteer = require(process.env.PUPPETEER_MODULE || 'puppeteer-core');

(async () => {
  const output = process.env.BROWSER_OUTPUT || 'tmp/management-cache';
  assert.ok(process.env.CHROMIUM_PATH, 'Set CHROMIUM_PATH to Chromium');
  fs.mkdirSync(output, {recursive: true});
  const source = path.join(output, 'cache-fixture.html');
  const versioned = path.join(output, 'cache-fixture-versioned.html');
  const html = '<!doctype html><div id="state"></div><script src="/koheron/instruments.js"></script>' +
    '<script>document.querySelector("#state").textContent=typeof RuntimeStream==="function"?"Current":"Outdated";</script>';
  fs.writeFileSync(source, html);
  const built = spawnSync('python3', ['web/version_assets.py', source, 'tmp/www', versioned], {encoding: 'utf8'});
  assert.equal(built.status, 0, built.stderr);
  const javascript = fs.readFileSync('tmp/www/instruments.js');
  let phase = 'old', unversionedReads = 0, versionedReads = 0;
  const server = http.createServer((request, response) => {
    const url = new URL(request.url, 'http://localhost');
    if (url.pathname === '/koheron/instruments.js') {
      const fingerprinted = url.searchParams.has('v');
      if (fingerprinted) ++versionedReads; else ++unversionedReads;
      response.writeHead(200, {'Content-Type': 'application/javascript',
        'Cache-Control': fingerprinted ? 'no-cache' : 'public, max-age=31536000'});
      response.end(phase === 'old' ? '// Older SDK bundle without RuntimeStream' : javascript);
    } else if (url.pathname === '/') {
      response.writeHead(200, {'Content-Type': 'text/html', 'Cache-Control': 'no-store'});
      response.end(phase === 'versioned' ? fs.readFileSync(versioned) : html);
    } else { response.writeHead(204); response.end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  let browser;
  const checks = [], errors = [];
  try {
    browser = await puppeteer.launch({executablePath: process.env.CHROMIUM_PATH, headless: true, args: ['--no-sandbox']});
    const page = await browser.newPage();
    page.on('pageerror', error => errors.push(error.message));
    const base = 'http://127.0.0.1:' + server.address().port;
    const visit = async name => {
      await page.goto(base + '/?visit=' + name, {waitUntil: 'networkidle0'});
      return page.$eval('#state', element => element.textContent);
    };
    assert.equal(await visit('old'), 'Outdated');
    phase = 'new';
    assert.equal(await visit('updated-without-version'), 'Outdated');
    assert.equal(unversionedReads, 1);
    checks.push('new HTML still uses the old cached bundle when its URL is unchanged');
    phase = 'versioned';
    assert.equal(await visit('updated-with-version'), 'Current');
    assert.equal(unversionedReads, 1); assert.equal(versionedReads, 1);
    checks.push('the generated content version bypasses the existing cached bundle');
    assert.equal(await visit('revalidated'), 'Current');
    assert.equal(versionedReads, 2);
    checks.push('no-cache revalidates the versioned bundle on the next navigation');
    assert.deepEqual(errors, []);
    fs.writeFileSync(path.join(output, 'cache-browser-results.json'),
      JSON.stringify({recorded_utc: new Date().toISOString(), checks, unversionedReads, versionedReads, errors}, null, 2) + '\n');
    for (const check of checks) console.log('PASS: ' + check);
  } finally {
    if (browser) await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
