class App {
    public control: DDSFrequency;
    private client: Client;
    private stopped = false;

    constructor(private window: Window, private document: Document, ip: string) {
        this.client = new Client(ip, 5);
        window.addEventListener('HTMLImportsLoaded', () => { void this.init(); });
        window.addEventListener('pagehide', () => this.dispose());
        window.addEventListener('pageshow', event => {
            if ((event as PageTransitionEvent).persisted && this.stopped) { window.location.reload(); }
        });
    }

    private async init(): Promise<void> {
        try {
            new Imports(this.document);
            await this.client.init();
            if (this.stopped) { return; }
            this.control = new DDSFrequency(this.document, new DualDDS(this.client));
            await this.control.init();
        } catch (error) {
            if (this.stopped) { return; }
            const status = this.document.getElementById('dds-frequency-status');
            status.textContent = 'DDS unavailable';
            status.title = String(error);
            this.dispose();
        }
    }

    private dispose(): void {
        if (this.stopped) { return; }
        this.stopped = true;
        this.control?.dispose();
        this.client.exit();
    }
}

let app = new App(window, document, location.hostname);
