// (c) Koheron
// KoheronLog: stream and display koheron-server logs

interface KoheronLogEntry {
    ts: number | string | null; // microseconds since epoch
    prio?: number;             // systemd priority 0..7
    msg: string;
    truncated?: boolean;
}

type KoheronLogCallback = (entries: KoheronLogEntry[], reset?: boolean) => void;

// A separate read-only stream lets Pause release its journal reader without
// interrupting instrument controls. HTTP uses the same bounded cursor protocol.
class KoheronLog {
    private cursor: string | null = null;
    private invocation: string = null;
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
    setInvocation(value: string): void {
        if (value === this.invocation) { return; }
        this.invocation = value; this.cursor = null; this.disconnect();
        if (this.running) { this.connect(); }
    }
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
        const query = new URLSearchParams();
        if (this.cursor) { query.set('cursor', this.cursor); }
        if (this.invocation) { query.set('invocation', this.invocation); }
        return this.endpoint + path + (query.size ? '?' + query.toString() : '');
    }
    private deliver(data: any, fallback: boolean): void {
        if (!data || data.type !== 'logs' || typeof data.reset !== 'boolean' ||
            (data.cursor !== null && (typeof data.cursor !== 'string' || data.cursor.length > 1024)) ||
            !Array.isArray(data.entries) || data.entries.length > 200 ||
            data.entries.some((entry: any) => !entry || typeof entry.msg !== 'string' || entry.msg.length > 12288 ||
                (entry.ts !== null && !(typeof entry.ts === 'number' && Number.isSafeInteger(entry.ts) && entry.ts >= 0) &&
                    !(typeof entry.ts === 'string' && /^\d{1,17}$/.test(entry.ts))) ||
                (entry.truncated != null && typeof entry.truncated !== 'boolean') ||
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

interface LogGroup {
    entry: KoheronLogEntry;
    firstTimestamp: KoheronLogEntry['ts'];
    count: number;
    row: HTMLSpanElement;
    time: HTMLSpanElement;
    repeats: Text;
}

// Keep existing rows and message nodes intact so live updates preserve selection.
class KoheronLogView {
    private groups: LogGroup[] = [];
    private query = '';
    private priority = 7;
    private empty: HTMLSpanElement;
    private clock = new Intl.DateTimeFormat(undefined, {hour12: false, timeStyle: 'medium'});

    constructor(private pre: HTMLPreElement, private follow: () => boolean,
                private count?: HTMLElement) {
        this.empty = pre.ownerDocument.createElement('span');
        this.empty.className = 'log-empty'; pre.appendChild(this.empty); this.updateCount();
    }
    clear(): void {
        this.groups.forEach(group => group.row.remove()); this.groups = []; this.updateCount();
    }
    private matches(entry: KoheronLogEntry): boolean {
        return (entry.prio ?? 6) <= this.priority && entry.msg.toLowerCase().includes(this.query);
    }
    private updateCount(): void {
        const visible = this.groups.filter(group => !group.row.hidden).length;
        this.empty.hidden = visible > 0;
        this.empty.textContent = this.groups.length ? 'No messages match these filters.' : 'No log messages yet.';
        if (this.count) { this.count.textContent = `${visible} of ${this.groups.length} groups`; }
    }
    private anchor(): {row: HTMLSpanElement; top: number; scroll: number} {
        const top = this.pre.getBoundingClientRect().top;
        const row = this.groups.find(group => !group.row.hidden && group.row.getBoundingClientRect().bottom > top)?.row;
        return {row, top: row?.getBoundingClientRect().top ?? 0, scroll: this.pre.scrollTop};
    }
    private restore(anchor: {row: HTMLSpanElement; top: number; scroll: number}): void {
        if (this.follow()) { this.pre.scrollTop = this.pre.scrollHeight; }
        else if (anchor.row?.isConnected && !anchor.row.hidden) {
            this.pre.scrollTop += anchor.row.getBoundingClientRect().top - anchor.top;
        } else { this.pre.scrollTop = anchor.scroll; }
    }
    filter(query: string, priority: number): void {
        const anchor = this.anchor();
        this.query = query.toLowerCase().trim(); this.priority = priority;
        this.groups.forEach(group => { group.row.hidden = !this.matches(group.entry); });
        this.updateCount(); this.restore(anchor);
    }
    append(entries: KoheronLogEntry[], reset = false): void {
        const anchor = this.anchor(), doc = this.pre.ownerDocument, fragment = doc.createDocumentFragment();
        if (reset) { this.clear(); }
        for (const entry of entries) {
            let group = this.groups[this.groups.length - 1];
            // Different long messages may share their truncated prefix.
            if (group && !entry.truncated && !group.entry.truncated && group.entry.msg === entry.msg &&
                (group.entry.prio ?? 6) === (entry.prio ?? 6)) {
                group.count++; group.entry = entry;
            } else {
                const row = doc.createElement('span'), time = doc.createElement('span'), message = doc.createElement('span');
                row.className = 'log-line'; time.className = 'log-time'; message.className = 'log-message';
                row.dataset.level = (entry.prio ?? 6) <= 3 ? 'error' : (entry.prio ?? 6) <= 4 ? 'warning' : 'info';
                message.textContent = entry.msg;
                const repeats = doc.createTextNode(''); row.append(time, message, repeats);
                if (entry.truncated) {
                    const marker = doc.createElement('span'); marker.className = 'log-truncated';
                    marker.textContent = ' [truncated]'; marker.title = 'Message shortened to fit the 4 KiB source limit.'; row.appendChild(marker);
                }
                row.hidden = !this.matches(entry);
                group = {entry, firstTimestamp: entry.ts, count: 1, row, time, repeats};
                this.groups.push(group); fragment.appendChild(row);
            }
            const latest = group.entry.ts == null ? null : new Date(Number(group.entry.ts) / 1000);
            const first = group.firstTimestamp == null ? null : new Date(Number(group.firstTimestamp) / 1000);
            group.time.textContent = `[${latest ? this.clock.format(latest) : '—'}] `;
            group.repeats.data = group.count > 1 ? `  (×${group.count})` : '';
            group.row.title = `First: ${first?.toISOString() ?? '—'}\nLatest: ${latest?.toISOString() ?? '—'}\nOccurrences: ${group.count}`;
        }
        this.pre.insertBefore(fragment, this.empty);
        while (this.groups.length > 1000) { this.groups.shift().row.remove(); }
        this.updateCount(); this.restore(anchor);
    }
    text(): string {
        return this.groups.filter(group => !group.row.hidden).map(group => group.row.textContent).join('\n');
    }
}

class KoheronLogWidget {
    constructor(document: Document, runtime?: RuntimeStream) {
        const pre = document.getElementById('koheron-log') as HTMLPreElement;
        const follow = document.getElementById('log-follow') as HTMLInputElement;
        const pause = document.getElementById('log-pause') as HTMLButtonElement;
        const status = document.getElementById('log-status');
        const notice = document.getElementById('log-notice');
        const search = document.getElementById('log-search') as HTMLInputElement;
        const severity = document.getElementById('log-severity') as HTMLSelectElement;
        const scope = document.getElementById('log-scope') as HTMLSelectElement;
        const view = new KoheronLogView(pre, () => follow.checked, document.getElementById('log-count'));
        const log = new KoheronLog('/api/logs/koheron', 1000, (entries, reset) => {
            if (reset) {
                notice.hidden = false; notice.textContent = 'Older log history is no longer available. Showing recent entries.';
            }
            view.append(entries, reset);
        }, (failed, fallback) => {
            status.textContent = failed ? 'Disconnected · retrying…' : fallback ? 'Live · polling' : 'Live';
            status.dataset.state = failed ? 'error' : 'live';
        });
        let invocation: string = null;
        const changeScope = () => {
            view.clear(); notice.hidden = true;
            log.setInvocation(scope.value === 'current' ? invocation : null);
            if (pause.getAttribute('aria-pressed') !== 'true') {
                status.textContent = 'Connecting…'; status.dataset.state = 'connecting';
            }
        };
        scope.addEventListener('change', changeScope);
        runtime?.subscribe(value => {
            const id = value.health?.instrument_service?.invocation;
            const next = typeof id === 'string' && /^[0-9a-f]{32}$/.test(id) && !/^0+$/.test(id) ? id : null;
            // A transient status error should not discard a known run ID.
            if (!next && value.health?.instrument_service?.error) { return; }
            scope.querySelector<HTMLOptionElement>('option[value="current"]').disabled = !next;
            if (next === invocation) { return; }
            invocation = next;
            if (scope.value === 'current') {
                if (!invocation) { scope.value = 'all'; }
                changeScope();
            }
        });
        const filter = () => view.filter(search.value, Number(severity.value));
        search.addEventListener('input', filter); severity.addEventListener('change', filter);
        pause.addEventListener('click', () => {
            const paused = pause.getAttribute('aria-pressed') !== 'true';
            pause.setAttribute('aria-pressed', String(paused)); pause.textContent = paused ? 'Resume' : 'Pause';
            status.textContent = paused ? 'Paused' : 'Connecting…'; status.dataset.state = paused ? 'paused' : 'connecting';
            if (paused) { log.stop(); } else { log.start(); }
        });
        pre.addEventListener('scroll', () => {
            if (pre.scrollHeight - pre.scrollTop - pre.clientHeight >= 24) { follow.checked = false; }
        });
        follow.addEventListener('change', () => { if (follow.checked) { pre.scrollTop = pre.scrollHeight; } });
        document.getElementById('log-download').addEventListener('click', () => {
            const url = URL.createObjectURL(new Blob([view.text()], {type: 'text/plain'}));
            const link = document.createElement('a'); link.href = url; link.download = 'koheron-instrument.log'; link.click();
            window.setTimeout(() => URL.revokeObjectURL(url), 1000);
        });
        log.start();
    }
}
