"""Exercise the actual browser RPC loop while completion is outstanding."""
from pathlib import Path
import shutil
import subprocess

import pytest


def test_browser_waits_quietly_and_can_cancel():
    if not shutil.which("node"):
        pytest.skip("Node is required for the browser collection check")
    script = Path(__file__).parents[1] / "web/bist.js"
    runner = r"""
const fs = require('fs'), vm = require('vm'), assert = require('assert');
const nodes = new Map(), calls = [], timers = new Map();
let nextTimer = 0, finish;
const node = () => ({textContent: '', scrollHeight: 0, scrollTop: 0, clientHeight: 0,
    replaceChildren() {}, append() {}});
const context = [
    {class: 'Bist', id: 7, functions: [
        {name: 'get_log', id: 1, ret_type: 'std::string'},
        {name: 'get_report', id: 2, ret_type: 'std::string'},
        {name: 'cancel', id: 3, ret_type: 'void'}]},
    {class: 'BistCompletion', id: 8, functions: [{name: 'wait', id: 1, ret_type: 'bool'}]}
];
class WebSocket {
    constructor(url) { this.url = url; queueMicrotask(() => this.onopen()); }
    send(packet) {
        const view = new DataView(packet), driver = view.getUint16(4), id = view.getUint16(6);
        const reply = value => {
            const text = typeof value === 'boolean' ? null : new TextEncoder().encode(JSON.stringify(value));
            const data = new ArrayBuffer(text ? 12 + text.length : 9), out = new DataView(data);
            if (text) { out.setUint32(8, text.length); new Uint8Array(data, 12).set(text); }
            else out.setUint8(8, Number(value));
            queueMicrotask(() => this.onmessage({data}));
        };
        if (driver === 1) { calls.push('context'); reply(context); }
        else if (driver === 8) {
            const ms = view.getUint32(8); calls.push(`wait:${ms}`);
            if (ms === 0) reply(false); else finish = () => reply(true);
        } else if (driver === 7 && id === 3) calls.push('cancel');
        else if (driver === 7 && id === 1) { calls.push('log'); reply({run_id: '1', cursor: 4, text: 'done'}); }
        else if (driver === 7 && id === 2) {
            calls.push('report'); reply({run_id: '1', state: 'complete', functional_status: 'pass', metrics: []});
        } else throw new Error(`Unexpected RPC ${driver}:${id}`);
    }
    close() {}
}
const browser = {WebSocket, TextDecoder, ArrayBuffer, DataView, Uint8Array,
    location: {protocol: 'http:', hostname: 'board'},
    document: {getElementById(id) { if (!nodes.has(id)) nodes.set(id, node()); return nodes.get(id); },
        createElement: node},
    window: {bistMetadata: {rows: () => ({summary: [], details: []})}},
    setTimeout(fn, ms) { const id = ++nextTimer; timers.set(id, {fn, ms}); return id; },
    clearTimeout(id) { timers.delete(id); }};
const flush = async () => { for (let i = 0; i < 20; ++i) await Promise.resolve(); };
(async () => {
    vm.runInNewContext(fs.readFileSync(process.argv[1], 'utf8'), browser);
    await flush();
    assert.deepStrictEqual(calls, ['context', 'wait:0', 'wait:60000']);
    assert.strictEqual(nodes.get('cancel').disabled, false);
    assert.strictEqual(nodes.get('download').disabled, true);
    assert([...timers.values()].some(timer => timer.ms === 65000));
    nodes.get('cancel').onclick(); await flush();
    assert.strictEqual(calls.at(-1), 'cancel');
    assert(!calls.includes('log') && !calls.includes('report'));
    finish(); await flush();
    assert(calls.includes('log') && calls.includes('report'));
    assert.strictEqual(nodes.get('state').textContent, 'complete');
    assert.strictEqual(nodes.get('start').disabled, false);
})().catch(error => { console.error(error); process.exitCode = 1; });
"""
    subprocess.run(["node", "-e", runner, str(script)], check=True, timeout=10)
