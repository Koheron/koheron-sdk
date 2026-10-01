// Exercise the actual ES5 worker source in an isolated worker-like realm.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const ts = require('typescript');
const context = vm.createContext({assert, console});
vm.runInContext(ts.transpileModule(fs.readFileSync(path.resolve(__dirname, '../web/plot/psd-stream.ts'), 'utf8'), {
    compilerOptions: {target: ts.ScriptTarget.ES5}
}).outputText, context);
const worker = vm.runInContext('PSDStream.run.toString()', context);
const sandbox = vm.createContext({assert, console});
vm.runInContext(`
let now = 0, nextId = 1, sockets = [], messages = [], timers = new Map();
globalThis.performance = {timeOrigin: 10000, now: () => now};
globalThis.setTimeout = (fn, delay) => { const id = nextId++; timers.set(id, {fn, due: now + delay}); return id; };
globalThis.clearTimeout = id => timers.delete(id);
globalThis.WebSocket = class {
    constructor(url) { this.url = url; this.readyState = 0; this.sent = []; sockets.push(this); }
    send(data) { this.sent.push(data); }
    close() { this.readyState = 3; }
};
globalThis.self = {postMessage(message, transfers) { messages.push({message, transfers}); }};
`, sandbox);
vm.runInContext('(' + worker + ')(self)', sandbox);
vm.runInContext(`
const tick = time => {
    now = time;
    for (const [id, event] of Array.from(timers)) {
        if (event.due <= now) { timers.delete(id); event.fn(); }
    }
};
const command = new Uint8Array(8); const header = new DataView(command.buffer);
header.setUint16(4, 5); header.setUint16(6, 7);
self.onmessage({data:{type:'init', url:'ws://test:8080', command, bins:2}});
let socket = sockets[0]; socket.readyState = 1; socket.onopen();
assert.equal(socket.sent.length, 1);
const response = value => {
    const buffer = new ArrayBuffer(16); const view = new DataView(buffer);
    view.setUint16(4,5); view.setUint16(6,7);
    new Float32Array(buffer,8).set([value,value+1]);
    socket.onmessage({data:buffer});
};
response(1);
assert.equal(messages.length,1);
assert.equal(messages[0].message.frames[0].time,10000);
assert.equal(new Float32Array(messages[0].message.frames[0].buffer,8)[0],1);
// No UI acknowledgement for four seconds: acquisition still proceeds.
for (let i=1; i<=260; i++) { tick(i*17); response(i); }
assert.equal(socket.sent.length,261);
assert.equal(messages.length,1); // One outstanding batch, never a postMessage flood.
self.onmessage({data:{type:'ack'}});
const batch=messages[1].message.frames;
assert.equal(batch.length,256); // Backpressure is bounded; oldest arrivals expire.
assert.equal(batch[0].time,10000+5*17);
assert.equal(batch.at(-1).time,10000+260*17);
assert.equal(messages[1].transfers.length,256);
assert.equal(new Float32Array(batch.at(-1).buffer,8)[0],260);
self.onmessage({data:{type:'ack'}});
self.onmessage({data:{type:'active',active:false}});
tick(now+1000); assert.equal(socket.sent.length,261);
self.onmessage({data:{type:'active',active:true}});
assert.equal(socket.sent.length,262);
self.onmessage({data:{type:'active',active:false}});
response(999); assert.equal(messages.length,2); // In-flight reply after pause is discarded.
self.onmessage({data:{type:'active',active:true}});
socket.onmessage({data:new ArrayBuffer(4)});
assert.equal(messages.at(-1).message.error,'Invalid spectrum response length');
assert.equal(socket.readyState,3);
tick(now+1000); assert.equal(sockets.length,2);
socket=sockets[1]; socket.readyState=1; socket.onopen();
tick(now+5000);
assert.equal(messages.at(-1).message.error,'Spectrum response timed out');
`, sandbox);
console.log('Worker wire format, original timestamps, bounded backpressure, pause, retry and timeout: PASS');
