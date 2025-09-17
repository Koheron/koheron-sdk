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

    /**
     * @param endpoint API root for logs (default "/api/logs/koheron")
     * @param pollInterval Polling interval in ms (default 1000)
     * @param maxLines How many lines to fetch for the first load (default 200)
     * @param onUpdate Optional callback called with new log entries
     */
    constructor(endpoint = "/api/logs/koheron",
                pollInterval = 1000,
                maxLines = 200,
                onUpdate?: KoheronLogCallback) {
        this.endpoint = endpoint;
        this.pollInterval = pollInterval;
        this.maxLines = maxLines;
        this.onUpdate = onUpdate;
    }

    /** Start polling logs */
    public start(): void {
        if (this.timer !== null) {
            // already running
            return;
        }

        this.pull(true);
    }

    /** Stop polling logs */
    public stop(): void {
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
        try {
            const url = this.cursor && !initial
                ? `${this.endpoint}/incr?cursor=${encodeURIComponent(this.cursor)}`
                : `${this.endpoint}?lines=${this.maxLines}`;

            const resp = await fetch(url, { cache: "no-store" });
            if (!resp.ok) {
                throw new Error(`Log fetch failed (${resp.status})`);
            }

            const data = await resp.json() as { cursor: string, entries: KoheronLogEntry[] };
            if (data.cursor) {
                this.cursor = data.cursor;
            }

            if (this.onUpdate && Array.isArray(data.entries) && data.entries.length > 0) {
                this.onUpdate(data.entries);
            }
        } catch (err) {
            console.error("KoheronLog error:", err);
        } finally {
            this.timer = window.setTimeout(() => this.pull(false), this.pollInterval);
        }
    }
}

class KoheronLogWidget {
    constructor (document: Document) {
        const pre = <HTMLPreElement>document.getElementById('koheron-log');
        const coalescingFormatter = this.makeCoalescingFormatter(pre);
        const logView = new KoheronLog(
            "/api/logs/koheron",   // <-- endpoint, not the element
            1000,                  // poll interval ms
            200,                   // initial lines
            (entries) => coalescingFormatter(entries)
        );
    
        logView.start();
    }

    formatTimestampShort(date: Date): string {
        const months = ["Jan","Feb","Mar","Apr","May","Jun",
                        "Jul","Aug","Sep","Oct","Nov","Dec"];
        const pad = (n: number) => n.toString().padStart(2, "0");
        return `${months[date.getMonth()]} ${pad(date.getDate())} ` +
              `${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
    }

    /**
     * Returns a callback you can pass to KoheronLog that:
     * - appends new lines into a <pre>
     * - coalesces consecutive identical messages by updating a trailing " ×N" counter
     */
    makeCoalescingFormatter(pre: HTMLPreElement) {
        let lastMsg = "";
        let lastTs = "";
        let repeatCount = 0;
        let lastLineStart = 0; // index in pre.textContent where the last line begins

        const render = (ts: string, msg: string, count: number) =>
        `[${ts}] ${msg}${count > 1 ? `  (×${count})` : ""}`;

        const setContent = (s: string) => { pre.textContent = s; };

        return (entries: KoheronLogEntry[]) => {
            if (pre.textContent == null) {
                setContent("");
            }

            for (const e of entries) {
                let ts = "";
                if (e.ts) {
                    const ms = Math.floor(parseInt(e.ts, 10) / 1000);
                    ts = this.formatTimestampShort(new Date(ms));
                }

                if (e.msg === lastMsg) {
                    repeatCount += 1;
                    const before = pre.textContent!.slice(0, lastLineStart);
                    const newLine = render(lastTs, lastMsg, repeatCount) + "\n";
                    setContent(before + newLine);
                } else {
                    repeatCount = 1;
                    lastMsg = e.msg;
                    lastTs = ts;

                    const txt = pre.textContent!;
                    lastLineStart = txt.length;
                    const newLine = render(ts, e.msg, repeatCount) + "\n";
                    setContent(txt + newLine);
                }
            }

            pre.scrollTop = pre.scrollHeight;
        };
    }
}