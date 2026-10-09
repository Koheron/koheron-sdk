// Poll independently of DOM work; bound queued data when the UI is busy.
class PSDStream {
    private worker: Worker;
    private stopped = false;

    constructor(host: string, command: Uint8Array<ArrayBuffer>, bins: number,
        frame: (psd: Float32Array, time: number) => void, error: (message: string) => void) {
        const url = URL.createObjectURL(new Blob(['(' + PSDStream.run.toString() + ')(self);'], {type: 'text/javascript'}));
        try { this.worker = new Worker(url); } finally { URL.revokeObjectURL(url); }
        this.worker.onmessage = event => {
            if (this.stopped) { return; }
            if (event.data.error) { error(event.data.error); return; }
            try {
                for (const received of event.data.frames) {
                    frame(new Float32Array(received.buffer, 8), (received.time - performance.timeOrigin) / 1000);
                }
            } finally { this.worker.postMessage({type: 'ack'}); }
        };
        this.worker.onerror = () => { if (!this.stopped) { error('Spectrum worker failed'); } };
        this.worker.postMessage({type: 'init', url: 'ws://' + host + ':8080', command, bins});
    }

    setActive(active: boolean): void { this.worker.postMessage({type: 'active', active}); }
    dispose(): void { this.stopped = true; this.worker.terminate(); }

    // Keep this function self-contained: its compiled source runs in a Worker.
    static run = function(scope: any): void {
        let socket: WebSocket;
        let config: {url: string; command: Uint8Array<ArrayBuffer>; bins: number};
        let active = true, busy = false, awaitingAck = false;
        let timer: any, timeout: any, started = 0;
        let frames: {buffer: ArrayBuffer; time: number}[] = [];
        const flush = () => {
            if (awaitingAck || !active || !frames.length) { return; }
            const batch = frames; frames = []; awaitingAck = true;
            scope.postMessage({frames: batch}, batch.map(frame => frame.buffer));
        };
        const poll = () => {
            if (!active || busy || !socket || socket.readyState !== 1) { return; }
            busy = true; started = performance.now();
            socket.send(config.command);
            timeout = setTimeout(() => fail('Spectrum response timed out'), 5000);
        };
        const fail = (message: string) => {
            clearTimeout(timer); clearTimeout(timeout); busy = false;
            if (socket) { socket.onclose = socket.onerror = null; socket.close(); }
            scope.postMessage({error: message});
            timer = setTimeout(connect, 1000);
        };
        const connect = () => {
            socket = new WebSocket(config.url); socket.binaryType = 'arraybuffer';
            socket.onopen = poll;
            socket.onerror = () => fail('Spectrum connection error');
            socket.onclose = () => fail('Spectrum connection closed');
            socket.onmessage = event => {
                clearTimeout(timeout); busy = false;
                const buffer = event.data;
                if (!(buffer instanceof ArrayBuffer) || buffer.byteLength !== 8 + config.bins * 4) {
                    fail('Invalid spectrum response length'); return;
                }
                const header = new DataView(buffer), command = new DataView(config.command.buffer, config.command.byteOffset, config.command.byteLength);
                if (header.getUint16(4) !== command.getUint16(4) || header.getUint16(6) !== command.getUint16(6)) {
                    fail('Unexpected spectrum response'); return;
                }
                if (active) {
                    frames.push({buffer, time: performance.timeOrigin + performance.now()});
                    // At most ~4 seconds wait for the UI; older data expires with
                    // its original time gap rather than growing an unbounded queue.
                    if (frames.length > 256) { frames.shift(); }
                    flush();
                    timer = setTimeout(poll, Math.max(0, 1000 / 60 - (performance.now() - started)));
                }
            };
        };
        scope.onmessage = (event: any) => {
            const data = event.data;
            if (data.type === 'init') { config = data; connect(); }
            if (data.type === 'ack') { awaitingAck = false; flush(); }
            if (data.type === 'active') {
                active = data.active; clearTimeout(timer);
                if (active) {
                    if (socket && socket.readyState === 1) { poll(); }
                    else if (!socket || socket.readyState > 1) { connect(); }
                } else { frames = []; }
            }
        };
    };
}
