class App {
    private imports: Imports;
    private signalGenerator: PhaseModulatorWidget;
    private stopped = false;
    private client: Client;
    public dds: DDS;
    private clockGenerator: ClockGenerator;
    private clockGeneratorApp: ClockGeneratorApp;
    private phaseNoiseAnalyzer: PhaseNoiseAnalyzer;
    private phaseNoiseAnalyzerApp: PhaseNoiseAnalyzerApp;
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
        const client = this.client = new Client(ip, sockpoolSize);

        window.addEventListener('HTMLImportsLoaded', async () => {
            try {
                await client.init();
                if (this.stopped) { return; }
                this.imports = new Imports(document);
                this.dds = new DDS(client);
                this.clockGenerator = new ClockGenerator(client);
                this.phaseNoiseAnalyzer = new PhaseNoiseAnalyzer(client);

                this.clockGeneratorApp = new ClockGeneratorApp(document, this.clockGenerator);
                this.phaseNoiseAnalyzerApp = new PhaseNoiseAnalyzerApp(document, this.phaseNoiseAnalyzer);

                await this.phaseNoiseAnalyzerApp.init();
                if (this.stopped) { return; }
                this.n_pts = this.phaseNoiseAnalyzerApp.nPoints;
                this.x_min = 100;
                this.x_max = 2E6;
                this.y_min = -200;
                this.y_max = 0;

                this.plotBasics = new PlotBasics(document, plot_placeholder, this.n_pts, this.x_min, this.x_max, this.y_min, this.y_max, this.phaseNoiseAnalyzer, "", "FREQUENCY OFFSET (Hz)");
                this.plot = new Plot(document, this.phaseNoiseAnalyzer, this.plotBasics);
                this.exportFile = new ExportFile(document, this.plot);
                for (const id of ['instrument-controls', 'plot-controls', 'laser-controls']) {
                    (document.getElementById(id) as HTMLFieldSetElement).disabled = false;
                }
                const status = document.getElementById('connection-status');
                status.textContent = 'Connected';
                status.dataset.state = 'live';

                // The analyzer establishes its 200 MS/s clock before the
                // generator reads metadata. Generator errors retain their own
                // retry control and leave acquisition available.
                this.signalGenerator = new PhaseModulatorWidget(
                    document.getElementById('phase-modulator'),
                    new PhaseModulatorDriver(client), {expectedChannels: 2});
                void this.signalGenerator.init().catch(() => {});

            } catch (error) {
                if (this.stopped) { return; }
                document.getElementById('connection-error').hidden = false;
                const status = document.getElementById('connection-status');
                status.textContent = 'Disconnected';
                status.dataset.state = 'error';
                console.error('Application initialization failed:', error);
                this.shutdown(document);
            }
        }, false);

        window.addEventListener('pagehide', () => this.shutdown(document));
    }

    private shutdown(document: Document): void {
        if (this.stopped) { return; }
        this.stopped = true;
        if (this.signalGenerator) { this.signalGenerator.dispose(); }
        if (this.plot) { this.plot.dispose(); }
        if (this.phaseNoiseAnalyzerApp) { this.phaseNoiseAnalyzerApp.dispose(); }
        for (const id of ['instrument-controls', 'plot-controls', 'laser-controls']) {
            (document.getElementById(id) as HTMLFieldSetElement).disabled = true;
        }
        this.client.exit();
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));
