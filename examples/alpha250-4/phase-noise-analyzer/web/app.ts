class App {
    private imports: Imports;
    private sampleRate: PnaSampleRate;
    private stopped = false;
    private client: Client;
    public dds: DDS;
    private clockGenerator: ClockGenerator;
    private clockGeneratorApp: ClockGeneratorApp;
    private phaseNoiseAnalyzer: PhaseNoiseAnalyzer;
    private phaseNoiseAnalyzerApp: PhaseNoiseAnalyzerApp;
    private phasePrecision: PhasePrecision;
    public plot: Plot;
    private plotBasics: PlotBasics;
    private exportFile: ExportFile;

    private n_pts: number;
    private x_min: number;
    private x_max: number;
    private y_min: number;
    private y_max: number;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 7;
        const client = this.client = new Client(ip, sockpoolSize, error => this.connectionFailed(document, error));

        window.addEventListener('HTMLImportsLoaded', async () => {
            try {
                await client.init();
                if (this.stopped) { return; }
                this.imports = new Imports(document);
                this.dds = new DDS(client);
                this.clockGenerator = new ClockGenerator(client);
                this.phaseNoiseAnalyzer = new PhaseNoiseAnalyzer(client);

                this.clockGeneratorApp = new ClockGeneratorApp(document, this.clockGenerator);
                this.phaseNoiseAnalyzerApp = new PhaseNoiseAnalyzerApp(document, this.phaseNoiseAnalyzer, error => this.connectionFailed(document, error));

                await this.phaseNoiseAnalyzerApp.init();
                this.phasePrecision = new PhasePrecision(client, document);
                await this.phasePrecision.init();
                if (this.stopped) { return; }
                this.sampleRate = new PnaSampleRate(document, this.phaseNoiseAnalyzer,
                    rate => this.phaseNoiseAnalyzerApp.setSampleRate(rate),
                    error => this.connectionFailed(document, error));
                await this.sampleRate.init();
                if (this.stopped) { return; }
                this.n_pts = this.phaseNoiseAnalyzerApp.nPoints;
                this.x_min = 100;
                this.x_max = 2E6;
                this.y_min = -200;
                this.y_max = 0;

                this.plotBasics = new PlotBasics(document, plot_placeholder, this.n_pts, this.x_min, this.x_max, this.y_min, this.y_max, this.phaseNoiseAnalyzer, "", "Offset frequency (Hz)");
                this.plot = new Plot(document, this.phaseNoiseAnalyzer, this.plotBasics, error => this.connectionFailed(document, error));
                this.exportFile = new ExportFile(document, this.plot);
                for (const id of ['instrument-controls', 'plot-controls']) {
                    (document.getElementById(id) as HTMLFieldSetElement).disabled = false;
                }
                const status = document.getElementById('connection-status');
                status.textContent = 'Connected';
                status.dataset.state = 'live';


            } catch (error) {
                if (this.stopped) { return; }
                this.connectionFailed(document, error);
            }
        }, false);

        window.addEventListener('pagehide', () => this.shutdown(document));
        // Back/Forward can restore the disposed page from the browser cache.
        // Start a fresh connection instead of leaving a frozen Connected view.
        window.addEventListener('pageshow', event => {
            if (event.persisted) { window.location.reload(); }
        });
    }

    private connectionFailed(document: Document, error: unknown): void {
        if (this.stopped) { return; }
        const status = document.getElementById('connection-status');
        const wasConnected = status.dataset.state === 'live';
        status.textContent = 'Disconnected';
        status.dataset.state = 'error';
        document.getElementById('connection-error').hidden = false;
        const message = document.getElementById('connection-error-message');
        if (message) { message.textContent = wasConnected
            ? 'Connection lost. The spectrum and readings are stale.'
            : 'Unable to connect to the phase-noise analyzer.'; }
        const average = document.getElementById('average-status');
        if (average) {
            average.textContent = '—/';
            average.dataset.state = 'unknown';
            average.title = 'Disconnected';
            average.setAttribute('aria-label', 'Average progress unavailable: disconnected');
        }
        document.querySelectorAll('.carrier-power-span, .phase-jitter-span, .time-jitter-span, #jitter-range, .tracking-state, .tracking-effective-bandwidth, .tracking-correction-x, .tracking-correction-y, #decade-values-table tbody td:last-child')
            .forEach(node => { node.textContent = '—'; });
        const performanceStatus = document.getElementById('performance-status');
        if (performanceStatus) { performanceStatus.textContent = 'Queue —'; performanceStatus.dataset.state = 'unknown'; performanceStatus.title = 'Processing status unavailable while disconnected'; }
        const coverage = document.getElementById('coverage-status');
        if (coverage) {
            coverage.textContent = 'Coverage —';
            coverage.dataset.state = 'unknown';
            coverage.title = 'Coverage unavailable while disconnected';
        }
        const precision = document.getElementById('precision-status');
        if (precision) {
            precision.textContent = '—';
            precision.dataset.state = 'unknown';
            precision.title = 'Acquisition status unavailable while disconnected';
        }
        if (this.plot) { this.plot.markUnavailable('Disconnected'); }
        console.error('Analyzer connection failed:', error);
        this.shutdown(document);
    }

    private shutdown(document: Document): void {
        if (this.stopped) { return; }
        this.stopped = true;
        if (this.plot) { this.plot.dispose(); }
        if (this.sampleRate) { this.sampleRate.dispose(); }
        if (this.phasePrecision) { this.phasePrecision.dispose(); }
        if (this.phaseNoiseAnalyzerApp) { this.phaseNoiseAnalyzerApp.dispose(); }
        for (const id of ['instrument-controls', 'plot-controls']) {
            (document.getElementById(id) as HTMLFieldSetElement).disabled = true;
        }
        this.client.exit();
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));
