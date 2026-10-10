interface RuntimeOperation {
    action: string; instrument: string; phase: string; busy: boolean;
    code: string; message: string; rollback: string; revision: number;
}
interface RuntimeStatus {
    type: 'status'; instruments: any; current_instrument: any;
    operation: RuntimeOperation; health: any;
}

// One connection shared by the instrument controls and system readouts.
class RuntimeStream {
    private socket: WebSocket;
    private timer: number;
    private watchdog: number;
    private fallbackTimer: number;
    private stopped = false;
    private request: AbortController;
    private generation = 0;
    private delivery = 0;
    private connected = false;
    private retryDelay = 1000;
    private listeners: Array<(status: RuntimeStatus) => void> = [];
    private errors: Array<(message: string) => void> = [];
    private last: RuntimeStatus;
    private lastError = '';

    constructor(private document: Document) {
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) { this.disconnect(); }
            else if (!this.stopped) { this.connect(); }
        });
        document.defaultView.addEventListener('pagehide', () => this.dispose());
        document.defaultView.addEventListener('pageshow', event => {
            if ((event as PageTransitionEvent).persisted) { this.stopped = false; this.connect(); }
        });
        this.connect();
    }
    subscribe(render: (status: RuntimeStatus) => void, error: (message: string) => void = () => {}): void {
        this.listeners.push(render); this.errors.push(error);
        if (this.last) { render(this.last); }
        if (this.lastError) { error(this.lastError); }
    }
    private deliver(value: RuntimeStatus): void {
        const object = (item: any) => !!item && typeof item === 'object' && !Array.isArray(item);
        const instrument = (item: any) => object(item) && typeof item.name === 'string' && item.name.length > 0 &&
            typeof item.version === 'string' && typeof item.is_default === 'boolean';
        const operation = value?.operation;
        if (!value || value.type !== 'status' || !value.instruments || !Array.isArray(value.instruments.instruments) ||
            !value.instruments.instruments.every(instrument) ||
            (value.current_instrument != null && !instrument(value.current_instrument)) ||
            (value.instruments.live_instrument != null && !instrument(value.instruments.live_instrument)) ||
            !object(operation) || typeof operation.busy !== 'boolean' || !Number.isSafeInteger(operation.revision) || operation.revision < 0 ||
            !['idle', 'validating', 'extracting', 'stopping', 'starting', 'restarting', 'rolling_back', 'succeeded', 'failed'].includes(operation.phase) ||
            ['action', 'instrument', 'code', 'message', 'rollback'].some(key => operation[key] != null && typeof operation[key] !== 'string') ||
            !object(value.health) || (value.health.load_average != null &&
                (!Array.isArray(value.health.load_average) || !value.health.load_average.every(Number.isFinite)))) {
            throw new Error('Invalid board status received.');
        }
        ++this.delivery; this.lastError = '';
        this.last = value;
        this.listeners.forEach(listener => listener(value));
    }
    private notifyError(message: string): void {
        this.lastError = message; this.errors.forEach(listener => listener(message));
    }
    private disconnect(): void {
        ++this.generation;
        if (this.request) { this.request.abort(); this.request = null; }
        const w = this.document.defaultView;
        w.clearTimeout(this.timer); w.clearTimeout(this.watchdog); w.clearTimeout(this.fallbackTimer);
        this.connected = false;
        if (this.socket) {
            const socket = this.socket; this.socket = null;
            socket.onclose = null; socket.onerror = null; socket.onmessage = null; socket.close();
        }
    }
    private connect(): void {
        if (this.stopped || this.document.hidden || this.socket) { return; }
        const w = this.document.defaultView;
        if (!w.WebSocket) { void this.refresh(); return; }
        const protocol = w.location.protocol === 'https:' ? 'wss:' : 'ws:';
        let socket: WebSocket;
        try { socket = this.socket = new w.WebSocket(`${protocol}//${w.location.host}/api/events`); }
        catch (_) { void this.refresh(); return; }
        const reconnect = () => {
            if (this.socket !== socket) { return; }
            this.disconnect();
            if (this.stopped || this.document.hidden) { return; }
            this.notifyError('Board connection lost. Reconnecting…');
            void this.refresh();
            this.timer = w.setTimeout(() => this.connect(), this.retryDelay);
            this.retryDelay = Math.min(10000, this.retryDelay * 2);
        };
        socket.onclose = reconnect; socket.onerror = reconnect;
        socket.onmessage = event => {
            if (this.socket !== socket) { return; }
            try {
                this.deliver(JSON.parse(event.data)); this.connected = true; this.retryDelay = 1000;
                w.clearTimeout(this.fallbackTimer);
                w.clearTimeout(this.watchdog);
                this.watchdog = w.setTimeout(reconnect, 7000);
            } catch (_) { reconnect(); }
        };
        this.watchdog = w.setTimeout(reconnect, 5000);
    }
    async refresh(): Promise<void> {
        if (this.request || this.stopped || this.document.hidden) { return; }
        const controller = this.request = new this.document.defaultView.AbortController();
        const generation = this.generation, delivery = this.delivery;
        const current = () => this.request === controller && generation === this.generation &&
            delivery === this.delivery && !this.stopped && !this.document.hidden;
        try {
            const value = await new Instruments().getRuntimeStatus(controller.signal);
            if (current()) { this.deliver(value); }
        }
        catch (error) { if (current()) { this.notifyError(String(error)); } }
        finally {
            if (this.request === controller) {
                this.request = null;
                if (!this.connected && !this.stopped && !this.document.hidden) {
                    this.document.defaultView.clearTimeout(this.fallbackTimer);
                    this.fallbackTimer = this.document.defaultView.setTimeout(() => this.refresh(), 2000);
                }
            }
        }
    }
    dispose(): void { this.stopped = true; this.disconnect(); }
}

class PreflightView {
    constructor(private document: Document, private container: HTMLElement) {}
    render(value: any): void {
        this.container.textContent = '';
        const status = this.document.createElement('p');
        status.className = 'status-line'; status.dataset.state = value.ready ? 'ready' : 'error';
        status.textContent = value.ready ? 'Ready to run' : value.message || 'Cannot run this instrument';
        this.container.appendChild(status);
        const table = this.document.createElement('table');
        const fields: Array<[string, string]> = [
            ['Board', value.metadata?.board || 'Unverified'],
            ['Architecture', value.metadata?.architecture || 'Older package'],
            ['Version', value.version || '—'],
            ['Staging space', `${PreflightView.bytes(value.required_bytes)} needed · ${PreflightView.bytes(value.available_bytes)} available`],
            ['Extracted size', PreflightView.bytes(value.archive?.extracted_bytes)]
        ];
        for (const [label, text] of fields) {
            const row = table.insertRow(); const heading = this.document.createElement('th');
            heading.scope = 'row'; heading.textContent = label; row.appendChild(heading); row.insertCell().textContent = text;
        }
        this.container.appendChild(table);
        for (const warning of value.warnings || []) {
            const note = this.document.createElement('p'); note.className = 'table-note'; note.textContent = warning; this.container.appendChild(note);
        }
    }
    static bytes(value: number): string {
        if (!Number.isFinite(value)) { return 'Unavailable'; }
        return value < 1024 * 1024 ? `${(value / 1024).toFixed(0)} KiB` : `${(value / 1024 / 1024).toFixed(1)} MiB`;
    }
}
