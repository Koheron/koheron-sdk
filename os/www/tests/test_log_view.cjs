const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {JSDOM} = require('jsdom');
const ts = require('../../../web/transpile.cjs');
const code = ts.transpileModule(fs.readFileSync('os/www/koheron_server_log.ts', 'utf8')).outputText;
function fixture(t) {
    const dom = new JSDOM('<pre></pre><output></output>', {runScripts: 'outside-only'});
    t.after(() => dom.window.close());
    const w = dom.window, pre = w.document.querySelector('pre'), count = w.document.querySelector('output');
    w.eval(code + '\nwindow.KoheronLogView = KoheronLogView;');
    return {w, pre, count, view: new w.KoheronLogView(pre, () => false, count)};
}
const entry = (msg, ts = null, prio = 6, truncated = false) => ({msg, ts, prio, truncated});

test('append and repetition preserve message nodes and text selection', t => {
    const {w, pre, view} = fixture(t);
    view.append([entry('selected message', 1700000000000000)]);
    const row = pre.querySelector('.log-line'), node = row.querySelector('.log-message').firstChild;
    const range = w.document.createRange();
    range.setStart(node, 0); range.setEnd(node, 8);
    const selection = w.getSelection(); selection.addRange(range);
    view.append([entry('selected message', 1700000002000000), entry('next')]);
    assert.equal(pre.querySelector('.log-line'), row);
    assert.equal(row.querySelector('.log-message').firstChild, node);
    assert.equal(selection.toString(), 'selected');
    assert.match(row.textContent, /×2/);
    assert.match(row.title, /First: 2023-11-14T22:13:20.000Z/);
    assert.match(row.title, /Latest: 2023-11-14T22:13:22.000Z/);
    assert.equal(row.querySelector('.log-time').textContent,
        '[' + new Intl.DateTimeFormat(undefined, {hour12: false, timeStyle: 'medium'}).format(new Date(1700000002000)) + '] ');
});

test('truncated prefixes remain separate and exported text includes the notice', t => {
    const {pre, view} = fixture(t);
    view.append([entry('<b>prefix', null, 3, true), entry('<b>prefix', null, 3, true)]);
    assert.equal(pre.querySelectorAll('.log-line').length, 2);
    assert.equal(pre.querySelectorAll('.log-truncated').length, 2);
    assert.equal(pre.querySelector('b'), null);
    assert.match(view.text(), /\[truncated\]/);
    assert.doesNotMatch(view.text(), /×2/);
});

test('row retention is bounded and exported lines reflect filters', t => {
    const {pre, view, count} = fixture(t);
    view.append(Array.from({length: 1200}, (_, i) => entry('message ' + i, null, i % 2 ? 3 : 6)));
    assert.equal(pre.querySelectorAll('.log-line').length, 1000);
    assert.equal(view.text().split('\n').length, 1000);
    assert.match(view.text().split('\n')[0], /message 200$/);
    view.filter('message 119', 3);
    assert.equal(count.textContent, '5 of 1000 groups');
    assert.equal(view.text().split('\n').length, 5);
    assert.doesNotMatch(view.text(), /message 1190/);
    view.append([entry('message 1199', null, 3)]);
    assert.match(view.text(), /×2/);
});

test('reset replaces history while retaining the current search and severity', t => {
    const {pre, view, count} = fixture(t);
    view.append([entry('old')]); view.filter('fault', 3);
    view.append([entry('fault', null, 3), entry('info')], true);
    assert.equal(count.textContent, '1 of 2 groups');
    assert.equal(view.text(), '[—] fault');
    assert.doesNotMatch(pre.textContent, /old/);
});

test('pruning above the viewport preserves its first retained visible row', t => {
    const {pre, view} = fixture(t);
    view.append(Array.from({length: 1000}, (_, i) => entry(String(i))));
    pre.scrollTop = 5000;
    pre.getBoundingClientRect = () => ({top: 0});
    for (const row of pre.querySelectorAll('.log-line')) {
        row.getBoundingClientRect = () => {
            const index = [...pre.querySelectorAll('.log-line')].indexOf(row);
            const top = index * 20 - pre.scrollTop;
            return {top, bottom: top + 20};
        };
    }
    const anchor = pre.querySelectorAll('.log-line')[250];
    view.append([entry('1000')]);
    assert.equal(anchor.getBoundingClientRect().top, 0);
    assert.equal(pre.scrollTop, 4980);
});
