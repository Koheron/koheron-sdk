class App {
    private imports: Imports;
    public plot: Plot;
    private plotBasics: PlotBasics;
    private fft: FFT;
    public fftApp: FFTApp;
    public ddsFrequency: DDSFrequency;
    private clockGenerator: ClockGenerator;
    private clockGeneratorApp: ClockGeneratorApp;
    private precisionDac: PrecisionDac;
    private precisionChannelsApp: PrecisionChannelsApp;
    private exportFile: ExportFile;
    private client: Client;
    private stopped: boolean = false;

    private n_pts: number;
    private x_min: number;
    private x_max: number;
    private y_min: number;
    private y_max: number;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 5;
        this.client = new Client(ip, sockpoolSize);

        window.addEventListener('HTMLImportsLoaded', async () => {
            try {
                await this.client.init();
                if (this.stopped) { return; }

                this.imports = new Imports(document);
                this.fft = new FFT(this.client);
                this.precisionDac = new PrecisionDac(this.client);
                this.clockGenerator = new ClockGenerator(this.client);

                await new Promise<void>(resolve => this.fft.init(resolve));
                if (this.stopped) { return; }

                this.fftApp = new FFTApp(document, this.fft);
                this.ddsFrequency = new DDSFrequency(document, this.fft);

                this.n_pts = this.fft.fft_size / 2;
                this.x_min = 0;
                this.x_max = this.fft.status.fs / 1E6 / 2;
                this.y_min = -200;
                this.y_max = 170;

                this.plotBasics = new PlotBasics(document, plot_placeholder, this.n_pts, this.x_min, this.x_max, this.y_min, this.y_max, this.fft, "", "Frequency (MHz)");
                this.plot = new Plot(document, this.fft, this.plotBasics);

                this.clockGeneratorApp = new ClockGeneratorApp(document, this.clockGenerator);
                this.precisionChannelsApp = new PrecisionChannelsApp(document, this.precisionDac);
                this.exportFile = new ExportFile(document, this.plot);

            } catch (err) {
                if (this.stopped) { return; }
                console.error('Application initialization failed:', err);
                this.shutdown();
            }
        }, false);

        window.addEventListener('pagehide', () => this.shutdown());
        window.addEventListener('beforeunload', () => this.shutdown());
    }

    private shutdown(): void {
        if (this.stopped) { return; }
        this.stopped = true;
        this.stop();
        this.client.exit();
    }

    private stop() {
        for (let key of Object.keys(this)) {
            const component = (this as any)[key];
            if (component && typeof component.dispose === 'function') {
                component.dispose();
            } else if (component && typeof component.stop === 'function') {
                component.stop();
            }
        }
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));
