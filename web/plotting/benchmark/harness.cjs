const fs = require('node:fs');
const path = require('node:path');
const ts = require('../../transpile.cjs');
const {chromium} = require('playwright-core');
const root = path.resolve(__dirname, '../../..');
const scripts = ['jquery.flot.js', 'jquery.flot.resize.js', 'jquery.flot.selection.js', 'jquery.flot.time.js', 'jquery.flot.axislabels.js', 'jquery.flot.canvas.js'];
async function launch() {
    return chromium.launch({executablePath: process.env.CHROME || (fs.existsSync('/usr/bin/google-chrome') ? '/usr/bin/google-chrome' : undefined), headless: true,
        args: ['--disable-gpu', '--disable-background-timer-throttling', '--disable-renderer-backgrounding', '--no-sandbox']});
}
async function load(browser, variant, scale = 1) {
    const page = await browser.newPage({viewport: {width: 1200, height: 800}, deviceScaleFactor: scale});
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.setContent('<!doctype html><html><body style="margin:8px;font:14px Arial"></body></html>');
    await page.addScriptTag({path: process.env.PLOT_JQUERY || path.join(__dirname, 'baseline/jquery.min.js')});
    if (!['original', 'previous', 'owned'].includes(variant)) throw new Error(`Unknown variant: ${variant}`);
    const dir = variant === 'original' ? path.join(__dirname, 'baseline') : path.join(root, `tmp/plotting/${variant === 'previous' ? 'comparison' : 'owned'}`);
    for (const name of scripts) await page.addScriptTag({path: path.join(dir, name)});
    const basicsPath = variant === 'owned' ? path.join(root, 'web/plot-basics/plot-basics.ts') : path.join(dir, 'plot-basics.ts');
    await page.addScriptTag({content: ts.transpileModule(fs.readFileSync(basicsPath, 'utf8'),
        {compilerOptions: {target: ts.ScriptTarget.ES2020}}).outputText});
    await page.addScriptTag({path: path.join(__dirname, 'workload.js')});
    return {page, errors};
}
module.exports = {launch, load, root, scripts, ts};
