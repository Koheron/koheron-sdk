interface LogRateSession {
    id: number;
    name: string;
    rx_total: number;
    tx_total: number;
    rx_mean: number;
    rx_inst: number;
    rx_max: number;
    tx_mean: number;
    tx_inst: number;
    tx_max: number;
}

interface LogRateResponse {
    ts: number; // Server steady-clock seconds, not a Unix timestamp.
    sessions: LogRateSession[];
}

type LogsRateCallback = (payload: LogRateResponse) => void;
type LogsRateErrorCallback = (err: Error) => void;

function rateNumber(value: number): number {
    return typeof value === 'number' && isFinite(value) && value > 0 ? value : 0;
}

// Rates arrive in bits/s, totals in bytes. Convert rates once at the caller.
function formatRateBytes(value: number, perSecond = false): string {
    const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
    let current = rateNumber(value);
    let unit = 0;
    while (current >= 1024 && unit < units.length - 1) { current /= 1024; unit++; }
    return current.toFixed(unit === 0 ? 0 : current < 10 ? 2 : 1) + ' ' + units[unit] + (perSecond ? '/s' : '');
}

class LogsRateClient {
    private timer: number | null = null;
    private running = false;
    private busy = false;
    private generation = 0;

    constructor(private endpoint: string, private pollInterval: number,
                private onUpdate: LogsRateCallback, private onError?: LogsRateErrorCallback) {}

    public start(): void {
        if (this.running) { return; }
        this.running = true;
        void this.poll();
    }

    public stop(): void {
        this.running = false;
        this.generation++;
        if (this.timer !== null) { window.clearTimeout(this.timer); this.timer = null; }
    }

    private async poll(): Promise<void> {
        if (!this.running || this.busy) { return; }
        this.busy = true;
        const generation = this.generation;
        const controller = new AbortController();
        const timeout = window.setTimeout(() => controller.abort(), 8000);
        try {
            if (document.hidden) { return; }
            const response = await fetch(this.endpoint, { cache: 'no-store', signal: controller.signal });
            if (!response.ok) { throw new Error('HTTP ' + response.status); }
            const data = await response.json() as LogRateResponse;
            if (!data || typeof data.ts !== 'number' || !isFinite(data.ts) || !Array.isArray(data.sessions) ||
                data.sessions.some(session => !session || typeof session.id !== 'number' ||
                    !isFinite(session.id) || typeof session.name !== 'string')) {
                throw new Error('Invalid rate data');
            }
            if (this.running && generation === this.generation) { this.onUpdate(data); }
        } catch (error) {
            if (this.running && generation === this.generation && this.onError) {
                this.onError(error instanceof Error ? error : new Error(String(error)));
            }
        } finally {
            window.clearTimeout(timeout);
            this.busy = false;
            if (this.running) { this.timer = window.setTimeout(() => this.poll(), this.pollInterval); }
        }
    }
}

class LogsRateChart {
    private ctx: CanvasRenderingContext2D;
    private width = 0;
    private height = 0;
    private samples: { ts: number; rx: number; tx: number }[] = [];
    private readonly windowSeconds = 240;

    constructor(private canvas: HTMLCanvasElement, private maxPoints = 120) {
        const context = canvas.getContext('2d');
        if (!context) { throw new Error('Cannot initialize chart context'); }
        this.ctx = context;
    }

    public resize(): void {
        const rect = this.canvas.getBoundingClientRect();
        this.width = rect.width;
        this.height = rect.height;
        if (!this.width || !this.height) { return; }
        const ratio = window.devicePixelRatio || 1;
        this.canvas.width = Math.round(this.width * ratio);
        this.canvas.height = Math.round(this.height * ratio);
        this.ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
        this.render();
    }

    public addSample(rx: number, tx: number, ts: number): void {
        const last = this.samples[this.samples.length - 1];
        if (last && ts === last.ts) { return; }
        if (last && ts < last.ts) { this.samples = []; }
        this.samples.push({ ts, rx: rateNumber(rx), tx: rateNumber(tx) });
        this.samples = this.samples.filter(sample => ts - sample.ts <= this.windowSeconds).slice(-this.maxPoints);
        this.render();
    }

    private render(): void {
        const ctx = this.ctx;
        const width = this.width;
        const height = this.height;
        if (width < 120 || height < 60) { return; }
        const left = 82, right = width - 12, top = 12, bottom = height - 30;
        ctx.clearRect(0, 0, width, height);
        const maximum = Math.max(4, ...this.samples.map(sample => Math.max(sample.rx, sample.tx))) * 1.1;
        ctx.font = '11px Lato, Arial, sans-serif';
        ctx.fillStyle = '#666'; ctx.strokeStyle = '#e6e6e6'; ctx.lineWidth = 1;
        ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
        for (let i = 0; i <= 4; i++) {
            const y = bottom - (bottom - top) * i / 4;
            ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke();
            ctx.fillText(formatRateBytes(maximum * i / 4, true), left - 8, y);
        }
        ctx.textBaseline = 'top';
        for (let i = 0; i <= 4; i++) {
            ctx.textAlign = i === 0 ? 'left' : i === 4 ? 'right' : 'center';
            ctx.fillText(i === 4 ? 'Latest' : `−${4 - i} min`, left + (right - left) * i / 4, bottom + 10);
        }
        if (!this.samples.length) {
            ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
            ctx.fillText('Waiting for rate data…', (left + right) / 2, (top + bottom) / 2);
            return;
        }
        const latest = this.samples[this.samples.length - 1].ts;
        const x = (ts: number) => right - (latest - ts) / this.windowSeconds * (right - left);
        const y = (value: number) => bottom - value / maximum * (bottom - top);
        ctx.save();
        ctx.beginPath(); ctx.rect(left, top - 2, right - left + 2, bottom - top + 4); ctx.clip();
        for (const series of [{ key: 'rx', color: '#1f77b4' }, { key: 'tx', color: '#cf791d' }]) {
            ctx.strokeStyle = series.color; ctx.fillStyle = series.color; ctx.lineWidth = 1.5;
            ctx.beginPath();
            this.samples.forEach((sample, index) => {
                const value = series.key === 'rx' ? sample.rx : sample.tx;
                if (index === 0 || sample.ts - this.samples[index - 1].ts > 6) { ctx.moveTo(x(sample.ts), y(value)); }
                else { ctx.lineTo(x(sample.ts), y(value)); }
            });
            ctx.stroke();
            // A marker also makes the first sample visible before a line exists.
            const sample = this.samples[this.samples.length - 1];
            ctx.beginPath(); ctx.arc(x(sample.ts), y(series.key === 'rx' ? sample.rx : sample.tx), 2, 0, 2 * Math.PI); ctx.fill();
        }
        ctx.restore();
    }
}

class LogsRateTable {
    constructor(private table: HTMLTableElement) {}

    public render(sessions: LogRateSession[]): void {
        this.table.innerHTML = '<caption class="sr-only">Session receive and transmit statistics</caption>' +
            '<thead><tr><th rowspan="2" scope="col">ID</th><th rowspan="2" scope="col">Socket</th>' +
            '<th colspan="4" scope="colgroup" class="rate-rx">RX · received by board</th>' +
            '<th colspan="4" scope="colgroup" class="rate-tx">TX · sent by board</th></tr>' +
            '<tr><th scope="col">Current</th><th scope="col">Mean</th><th scope="col">Peak</th><th scope="col">Total</th>' +
            '<th scope="col">Current</th><th scope="col">Mean</th><th scope="col">Peak</th><th scope="col">Total</th></tr></thead>';
        const body = this.table.createTBody();
        if (!sessions.length) {
            const cell = body.insertRow().insertCell();
            cell.colSpan = 10; cell.className = 'rate-empty'; cell.textContent = 'No connected sessions';
        }
        sessions.slice().sort((a, b) => a.id - b.id).forEach(session => {
            const row = body.insertRow();
            row.insertCell().textContent = String(session.id);
            row.insertCell().textContent = session.name;
            for (const direction of ['rx', 'tx']) {
                for (const metric of ['inst', 'mean', 'max', 'total']) {
                    const value = session[direction + '_' + metric];
                    row.insertCell().textContent = formatRateBytes(metric === 'total' ? value : value / 8, metric !== 'total');
                }
            }
        });
    }
}

class LogsRatePage {
    private table: LogsRateTable;
    private chart: LogsRateChart;
    private client: LogsRateClient;
    private lastTimestamp: number | null = null;
    private lastChange = 0;
    private lastUpdate = '';

    constructor(private document: Document) {
        const canvas = document.getElementById('logs-rate-chart') as HTMLCanvasElement;
        this.table = new LogsRateTable(document.getElementById('logs-rate-table') as HTMLTableElement);
        this.chart = new LogsRateChart(canvas);
        this.chart.resize();
        const resize = () => this.chart.resize();
        window.addEventListener('resize', resize);
        this.client = new LogsRateClient('/run/rates/sessions.json', 2000,
            payload => this.handleUpdate(payload), error => this.handleError(error));
        const pause = document.getElementById('logs-rate-pause');
        pause.addEventListener('click', () => {
            const paused = pause.getAttribute('aria-pressed') !== 'true';
            pause.setAttribute('aria-pressed', String(paused)); pause.textContent = paused ? 'Resume' : 'Pause';
            this.status(paused ? 'Paused' : 'Connecting…', paused ? 'paused' : 'connecting');
            if (paused) { this.client.stop(); } else { this.client.start(); }
        });
        window.addEventListener('pagehide', event => {
            if (!event.persisted) { this.client.stop(); window.removeEventListener('resize', resize); }
        });
        this.client.start();
    }

    private status(message: string, state: string): void {
        const element = this.document.getElementById('logs-rate-status');
        element.textContent = message; element.dataset.state = state;
        this.document.getElementById('logs-rate-readouts').dataset.stale = String(state !== 'live');
    }

    private handleUpdate(payload: LogRateResponse): void {
        if (this.lastTimestamp === payload.ts) {
            const stale = performance.now() - this.lastChange > 6000;
            this.status(stale ? 'No new data · last update ' + this.lastUpdate : 'Live', stale ? 'error' : 'live');
            return;
        }
        this.lastTimestamp = payload.ts;
        this.lastChange = performance.now();
        this.lastUpdate = new Date().toLocaleTimeString();
        this.table.render(payload.sessions);
        let rx = 0, tx = 0, rxTotal = 0, txTotal = 0, active = 0;
        payload.sessions.forEach(session => {
            rx += rateNumber(session.rx_inst); tx += rateNumber(session.tx_inst);
            rxTotal += rateNumber(session.rx_total); txTotal += rateNumber(session.tx_total);
            if (rateNumber(session.rx_inst) || rateNumber(session.tx_inst)) { active++; }
        });
        this.chart.addSample(rx / 8, tx / 8, payload.ts);
        const values = { 'rate-sessions': String(payload.sessions.length), 'rate-active': String(active),
            'rate-rx': formatRateBytes(rx / 8, true), 'rate-tx': formatRateBytes(tx / 8, true),
            'rate-rx-total': formatRateBytes(rxTotal), 'rate-tx-total': formatRateBytes(txTotal) };
        Object.keys(values).forEach(id => this.document.getElementById(id).textContent = values[id]);
        this.status('Live', 'live');
    }

    private handleError(error: Error): void {
        this.status((error.message === 'HTTP 404' ? 'Rate data unavailable' : 'Cannot load rates') + ' · retrying…', 'error');
    }
}
