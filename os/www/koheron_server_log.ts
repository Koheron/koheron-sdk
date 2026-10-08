// (c) Koheron
// KoheronLog: fetch and display koheron-server logs

interface KoheronLogEntry {
    ts: string | null;    // microseconds since epoch (string)
    prio: number;         // systemd priority 0..7
    msg: string;
}

type KoheronLogCallback = (entries: KoheronLogEntry[]) => void;

class KoheronLog {
    private endpoint: string;
    private cursor: string | null = null;
    private timer: number | null = null;
    private pollInterval: number;
    private maxLines: number;
    private onUpdate?: KoheronLogCallback;
    private running = false;
    private busy = false;

    /**
     * @param endpoint API root for logs (default "/api/logs/koheron")
     * @param pollInterval Polling interval in ms (default 1000)
     * @param maxLines How many lines to fetch for the first load (default 200)
     * @param onUpdate Optional callback called with new log entries
     */
    constructor(endpoint = "/api/logs/koheron",
                pollInterval = 1000,
                maxLines = 200,
                onUpdate?: KoheronLogCallback,
                private onState: (error: boolean) => void = () => {}) {
        this.endpoint = endpoint;
        this.pollInterval = pollInterval;
        this.maxLines = maxLines;
        this.onUpdate = onUpdate;
    }

    /** Start polling logs */
    public start(): void {
        if (this.running) { return; }
        this.running = true;
        this.pull(this.cursor === null);
    }

    /** Stop polling logs */
    public stop(): void {
        this.running = false;
        if (this.timer !== null) {
            window.clearTimeout(this.timer);
            this.timer = null;
        }
    }

    /** Manually force a refresh (does not affect the interval) */
    public refresh(): void {
        this.pull(this.cursor === null);
    }

    private async pull(initial: boolean): Promise<void> {
        if (!this.running || this.busy) { return; }
        if (this.timer !== null) { window.clearTimeout(this.timer); this.timer = null; }
        this.busy = true;
        try {
            if (document.hidden) { return; }
            const url = this.cursor && !initial
                ? `${this.endpoint}/incr?cursor=${encodeURIComponent(this.cursor)}`
                : `${this.endpoint}?lines=${this.maxLines}`;

            const resp = await fetch(url, { cache: "no-store" });
            if (!resp.ok) {
                throw new Error(`Log fetch failed (${resp.status})`);
            }

            const data = await resp.json() as { cursor: string, entries: KoheronLogEntry[] };
            if (!this.running) { return; }
            this.onState(false);
            if (data.cursor) {
                this.cursor = data.cursor;
            }

            if (this.onUpdate && Array.isArray(data.entries) && data.entries.length > 0) {
                this.onUpdate(data.entries);
            }
        } catch (err) {
            if (this.running) { this.onState(true); }
        } finally {
            this.busy = false;
            if (this.running) {
                this.timer = window.setTimeout(() => this.pull(false), this.pollInterval);
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
        const log = new KoheronLog('/api/logs/koheron', 1000, 200, format, failed => {
            status.textContent = failed ? 'Disconnected · retrying…' : 'Live';
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
        window.addEventListener('pagehide', event => { if (!event.persisted) { log.stop(); } });
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
        return (entries: KoheronLogEntry[]) => {
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
