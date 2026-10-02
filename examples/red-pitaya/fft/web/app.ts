class App {
    private imports: Imports;
    public plot: Plot;
    private plotBasics: PlotBasics;
    private fft: FFT;
    public fftApp: FFTApp;
    public ddsFrequency: DDSFrequency;
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

                await this.fft.init();
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

                this.exportFile = new ExportFile(document, this.plot, 'Red Pitaya');
                (document.getElementById('instrument-controls') as HTMLFieldSetElement).disabled = false;
                const pause = document.getElementById('pause-display') as HTMLButtonElement;
                const reset = document.getElementById('reset-view') as HTMLButtonElement;
                pause.disabled = reset.disabled = false;
                pause.addEventListener('click', () => {
                    const paused = pause.getAttribute('aria-pressed') !== 'true';
                    pause.setAttribute('aria-pressed', String(paused));
                    pause.textContent = paused ? 'Resume' : 'Pause';
                    this.plot.setPaused(paused);
                });
                reset.addEventListener('click', () => plot_placeholder.trigger('dblclick'));


            } catch (err) {
                if (this.stopped) { return; }
                document.getElementById('connection-error').hidden = false;
                const status = document.getElementById('connection-status');
                status.textContent = 'Disconnected';
                status.dataset.state = 'error';
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
