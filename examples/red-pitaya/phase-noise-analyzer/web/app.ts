class App {
    private signalGenerator: PhaseModulatorWidget;
    private stopped = false;
    private client: Client;
    private phaseNoiseAnalyzer: PhaseNoiseAnalyzer;
    private phaseNoiseAnalyzerApp: PhaseNoiseAnalyzerApp;
    private phasePrecision: PhasePrecision;
    public plot: Plot;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 7;
        const client = this.client = new Client(ip, sockpoolSize, error => this.connectionFailed(document, error));

        window.addEventListener('HTMLImportsLoaded', async () => {
            try {
                await client.init();
                if (this.stopped) { return; }
                new Imports(document);
                this.phaseNoiseAnalyzer = new PhaseNoiseAnalyzer(client);

                this.phaseNoiseAnalyzerApp = new PhaseNoiseAnalyzerApp(document, this.phaseNoiseAnalyzer, error => this.connectionFailed(document, error));

                await this.phaseNoiseAnalyzerApp.init();
                this.phasePrecision = new PhasePrecision(client, document);
                await this.phasePrecision.init();
                if (this.stopped) { return; }
                const plotBasics = new PlotBasics(document, plot_placeholder,
                    this.phaseNoiseAnalyzerApp.nPoints, 100, 2E6, -200, 0,
                    this.phaseNoiseAnalyzer, "", "Offset frequency (Hz)");
                this.plot = new Plot(document, this.phaseNoiseAnalyzer, plotBasics, error => this.connectionFailed(document, error));
                new ExportFile(document, this.plot);
                for (const id of ['instrument-controls', 'settings-controls', 'plot-controls', 'laser-controls']) {
                    (document.getElementById(id) as HTMLFieldSetElement).disabled = false;
                }
                const status = document.getElementById('connection-status');
                status.textContent = 'Connected';
                status.dataset.state = 'live';

                // Generator errors retain their own retry control and leave
                // analyzer acquisition available.
                this.signalGenerator = new PhaseModulatorWidget(
                    document.getElementById('phase-modulator'),
                    new PhaseModulatorDriver(client), {expectedChannels: 2, halfScaleOutput: true});
                void this.signalGenerator.init().catch(() => {});

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
        showPnaConnectionError(document);
        if (this.plot) { this.plot.markUnavailable('Disconnected'); }
        console.error('Analyzer connection failed:', error);
        this.shutdown(document);
    }

    private shutdown(document: Document): void {
        if (this.stopped) { return; }
        this.stopped = true;
        if (this.signalGenerator) { this.signalGenerator.dispose(); }
        if (this.plot) { this.plot.dispose(); }
        if (this.phaseNoiseAnalyzerApp) { this.phaseNoiseAnalyzerApp.dispose(); }
        if (this.phasePrecision) { this.phasePrecision.dispose(); }
        for (const id of ['instrument-controls', 'settings-controls', 'plot-controls', 'laser-controls']) {
            (document.getElementById(id) as HTMLFieldSetElement).disabled = true;
        }
        this.client.exit();
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));
