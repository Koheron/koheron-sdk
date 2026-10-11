// Exercise the shared pool with independently ordered opens and replies.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const test = require('node:test');
const ts = require('../transpile.cjs');

function fixture() {
    const sockets = [];
    class Socket extends EventTarget {
        constructor() { super(); this.readyState = 0; sockets.push(this); }
        open() { this.readyState = 1; this.onopen?.({}); }
        send(data) { this.sent = data; }
        close() { this.readyState = 3; this.onclose?.({}); this.dispatchEvent(new Event('close')); }
        reply(value) {
            const data = new ArrayBuffer(12);
            new DataView(data).setUint32(8, value);
            this.dispatchEvent(new MessageEvent('message', {data}));
        }
    }
    const context = vm.createContext({console, setTimeout, clearTimeout,
        WebSocket: Socket, window: {}, ArrayBuffer, DataView});
    vm.runInContext(ts.transpileModule(fs.readFileSync('web/koheron.ts', 'utf8'), {
        compilerOptions: {target: ts.ScriptTarget.ES2020}
    }).outputText, context);
    const Pool = vm.runInContext('WebSocketPool', context);
    const Client = vm.runInContext('Client', context);
    return {sockets, Pool, Client};
}

test('socket IDs survive out-of-order opens and simultaneous replies', async () => {
    const {sockets, Pool, Client} = fixture();
    let opened = 0, disconnected = 0;
    const pool = new Pool(2, 'ws://fixture', () => ++opened, () => ++disconnected);
    sockets[1].open(); sockets[0].open();
    assert.equal(opened, 1);
    const ids = [];
    pool.requestSocket(id => ids.push(id)); pool.requestSocket(id => ids.push(id));
    assert.deepEqual(ids, [0, 1]);
    assert.equal(pool.getSocket(0), sockets[0]);
    assert.equal(pool.getSocket(1), sockets[1]);
    ids.forEach(id => pool.freeSocket(id));

    const client = new Client('fixture', 2);
    client.websockpool = pool; client.connected = true;
    const first = client.readUint32({data: new Uint8Array([10])});
    const second = client.readUint32({data: new Uint8Array([20])});
    assert.deepEqual(Array.from(sockets[1].sent), [10]);
    assert.deepEqual(Array.from(sockets[0].sent), [20]);
    sockets[0].reply(200); sockets[1].reply(100);
    assert.deepEqual(await Promise.all([first, second]), [100, 200]);
    sockets[0].close();
    assert.equal(disconnected, 1);
    pool.exit();
    assert.equal(disconnected, 1);
});

test('exit before opening never initializes the pool', () => {
    const {sockets, Pool} = fixture();
    let opened = false;
    const pool = new Pool(2, 'ws://fixture', () => { opened = true; });
    pool.exit(); sockets[1].open(); sockets[0].open();
    assert.equal(opened, false);
    pool.requestSocket(id => assert.equal(id, -1));
});
