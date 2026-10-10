// (c) Koheron
// KoheronLog: stream and display koheron-server logs

interface KoheronLogEntry {
    ts: number | string | null; // microseconds since epoch
    prio?: number;             // systemd priority 0..7
    msg: string;
}

type KoheronLogCallback = (entries: KoheronLogEntry[], reset?: boolean) => void;

// A separate read-only stream lets Pause release its journal reader without
// interrupting instrument controls. HTTP uses the same bounded cursor protocol.
class KoheronLog {
    private cursor: string | null = null;
    private socket: WebSocket = null;
    private timer: number;
    private retryTimer: number;
    private watchdog: number;
    private request: AbortController = null;
    private running = false;
    private connected = false;
    private generation = 0;
    private retryDelay = 1000;

    constructor(private endpoint = '/api/logs/koheron',
                private pollInterval = 1000,
                private onUpdate: KoheronLogCallback = () => {},
                private onState: (error: boolean, fallback?: boolean) => void = () => {}) {
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) { this.disconnect(); }
            else if (this.running) { this.connect(); }
        });
        window.addEventListener('pagehide', () => this.disconnect());
        window.addEventListener('pageshow', event => {
            if (event.persisted && this.running) { this.connect(); }
        });
    }
    start(): void {
        if (this.running) { return; }
        this.running = true; this.connect();
    }
    stop(): void { this.running = false; this.disconnect(); }
    refresh(): void { if (!this.connected) { void this.pull(); } }
    private disconnect(): void {
        ++this.generation; this.connected = false;
        window.clearTimeout(this.timer); window.clearTimeout(this.retryTimer); window.clearTimeout(this.watchdog);
        if (this.request) { this.request.abort(); this.request = null; }
        if (this.socket) {
            const socket = this.socket; this.socket = null;
            socket.onmessage = null; socket.onclose = null; socket.onerror = null; socket.close();
        }
    }
    private url(path: string): string {
        return this.endpoint + path + (this.cursor ? '?cursor=' + encodeURIComponent(this.cursor) : '');
    }
    private deliver(data: any, fallback: boolean): void {
        if (!data || data.type !== 'logs' || typeof data.reset !== 'boolean' ||
            (data.cursor !== null && (typeof data.cursor !== 'string' || data.cursor.length > 1024)) ||
            !Array.isArray(data.entries) || data.entries.length > 200 ||
            data.entries.some((entry: any) => !entry || typeof entry.msg !== 'string' || entry.msg.length > 12288 ||
                (entry.ts !== null && !(typeof entry.ts === 'number' && Number.isSafeInteger(entry.ts) && entry.ts >= 0) &&
                    !(typeof entry.ts === 'string' && /^\d{1,17}$/.test(entry.ts))) ||
                (entry.prio != null && (!Number.isInteger(entry.prio) || entry.prio < 0 || entry.prio > 7))) ||
            (data.entries.length > 0 && !data.cursor)) { throw new Error('Invalid log batch'); }
        // Heartbeats repeat the current cursor. Never render a batch twice.
        if (data.reset || data.cursor !== this.cursor) { this.onUpdate(data.entries, data.reset); }
        this.cursor = data.cursor; this.onState(false, fallback);
    }
    private connect(): void {
        if (!this.running || document.hidden || this.socket) { return; }
        const reconnect = () => {
            this.disconnect();
            if (!this.running || document.hidden) { return; }
            this.onState(true); void this.pull();
            this.retryTimer = window.setTimeout(() => this.connect(), this.retryDelay);
            this.retryDelay = Math.min(10000, this.retryDelay * 2);
        };
        let socket: WebSocket;
        const requestedCursor = this.cursor;
        try {
            const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
            socket = this.socket = new WebSocket(protocol + '//' + location.host + this.url('/events'));
        } catch (_) { reconnect(); return; }
        const failed = () => { if (this.socket === socket) { reconnect(); } };
        socket.onclose = failed; socket.onerror = failed;
        socket.onmessage = event => {
            if (this.socket !== socket || !this.running || document.hidden) { return; }
            try {
                if (typeof event.data !== 'string' || event.data.length > 65536) { throw new Error('Oversized log batch'); }
                // If HTTP advanced the cursor while this handshake was pending,
                // reopen at that cursor rather than replaying the older WS batch.
                if (!this.connected && this.cursor !== requestedCursor) {
                    this.disconnect(); this.connect(); return;
                }
                this.deliver(JSON.parse(event.data), false);
                this.connected = true; this.retryDelay = 1000;
                if (this.request) { this.request.abort(); this.request = null; }
                window.clearTimeout(this.timer); window.clearTimeout(this.watchdog);
                this.watchdog = window.setTimeout(failed, 7000);
            } catch (_) { failed(); }
        };
        this.watchdog = window.setTimeout(failed, 5000);
    }
    private async pull(): Promise<void> {
        if (!this.running || document.hidden || this.connected || this.request) { return; }
        window.clearTimeout(this.timer);
        const controller = this.request = new AbortController(), generation = this.generation;
        const current = () => this.request === controller && this.generation === generation &&
            this.running && !document.hidden && !this.connected;
        const timeout = window.setTimeout(() => {
            if (current()) { this.onState(true); }
            controller.abort();
        }, 8000);
        try {
            const response = await fetch(this.url('/tail'), {cache: 'no-store', signal: controller.signal});
            if (!response.ok) { throw new Error('Log read failed'); }
            const bytes = await response.text();
            if (bytes.length > 65536) { throw new Error('Oversized log batch'); }
            if (current() && !controller.signal.aborted) { this.deliver(JSON.parse(bytes), true); }
        } catch (_) { if (current()) { this.onState(true); } }
        finally {
            window.clearTimeout(timeout);
            if (this.request === controller) {
                this.request = null;
                if (this.running && !document.hidden && !this.connected) {
                    this.timer = window.setTimeout(() => this.pull(), this.pollInterval);
                }
            }
        }
    }
}

class KoheronLogWidget {
    constructor(document: Document) {
        const pre = document.getElementById('koheron-log') as HTMLPreElement;
        const follow = document.getElementById('log-follow') as HTMLInputElement;
        const pause = document.getElementById('log-pause') as HTMLButtonElement;
        const status = document.getElementById('log-status');
        const format = this.makeCoalescingFormatter(pre, () => follow.checked);
        const log = new KoheronLog('/api/logs/koheron', 1000, format, (failed, fallback) => {
            status.textContent = failed ? 'Disconnected · retrying…' : fallback ? 'Live · polling' : 'Live';
            status.dataset.state = failed ? 'error' : 'live';
        });
        pause.addEventListener('click', () => {
            const paused = pause.getAttribute('aria-pressed') !== 'true';
            pause.setAttribute('aria-pressed', String(paused));
            pause.textContent = paused ? 'Resume' : 'Pause';
            status.textContent = paused ? 'Paused' : 'Connecting…';
            status.dataset.state = paused ? 'paused' : 'connecting';
            if (paused) { log.stop(); } else { log.start(); }
        });
        pre.addEventListener('scroll', () => {
            follow.checked = pre.scrollHeight - pre.scrollTop - pre.clientHeight < 24;
        });
        follow.addEventListener('change', () => {
            if (follow.checked) { pre.scrollTop = pre.scrollHeight; }
        });
        document.getElementById('log-download').addEventListener('click', () => {
            const url = URL.createObjectURL(new Blob([pre.textContent || ''], { type: 'text/plain' }));
            const link = document.createElement('a');
            link.href = url; link.download = 'koheron-instrument.log'; link.click();
            window.setTimeout(() => URL.revokeObjectURL(url), 1000);
        });
        log.start();
    }

    makeCoalescingFormatter(pre: HTMLPreElement, follow: () => boolean = () => true) {
        const lines: { message: string; timestamp: string; count: number; priority: number }[] = [];
        const render = () => {
            const scrollTop = pre.scrollTop;
            const shouldFollow = follow();
            const fragment = pre.ownerDocument.createDocumentFragment();
            lines.forEach((line, index) => {
                const row = pre.ownerDocument.createElement('span');
                row.className = 'log-line';
                row.dataset.level = line.priority <= 3 ? 'error' : line.priority <= 4 ? 'warning' : 'info';
                const timestamp = pre.ownerDocument.createElement('span');
                timestamp.className = 'log-time'; timestamp.textContent = `[${line.timestamp}] `;
                row.appendChild(timestamp);
                row.appendChild(pre.ownerDocument.createTextNode(line.message + (line.count > 1 ? `  (×${line.count})` : '')));
                fragment.appendChild(row);
                if (index < lines.length - 1) { fragment.appendChild(pre.ownerDocument.createTextNode('\n')); }
            });
            pre.textContent = '';
            pre.appendChild(fragment);
            pre.scrollTop = shouldFollow ? pre.scrollHeight : scrollTop;
        };
        return (entries: KoheronLogEntry[], reset = false) => {
            if (reset) { lines.length = 0; }
            for (const entry of entries) {
                const last = lines[lines.length - 1];
                const priority = typeof entry.prio === 'number' ? entry.prio : 6;
                if (last && last.message === entry.msg && last.priority === priority) { last.count++; }
                else {
                    const date = entry.ts ? new Date(Number(entry.ts) / 1000) : null;
                    lines.push({ message: entry.msg, priority,
                        timestamp: date ? date.toLocaleTimeString([], { hour12: false }) : '—', count: 1 });
                }
            }
            if (lines.length > 1000) { lines.splice(0, lines.length - 1000); }
            render();
        };
    }
}
