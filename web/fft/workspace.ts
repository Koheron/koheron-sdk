interface FFTBoardControls {
    init(): Promise<void>;
    precisionDacChanged?(values: ArrayLike<number>): void;
    dispose(): void;
}

interface FFTWorkspaceOptions {
    boardName: string;
    instrumentName?: string;
    signalGenerator?: boolean;
    halfScaleOutput?: boolean;
    createDriver: (client: Client) => FFTDriver;
    createBoard: (client: Client, document: Document, driver: FFTDriver) => FFTBoardControls;
}

// FFT boards share this lifecycle, acquisition controls, plot, actions and exports.
class FFTWorkspace {
    public plot: Plot;
    public fftApp: FFTApp;
    private fft: FFTDriver;
    private generator: PhaseModulatorWidget;
    private board: FFTBoardControls;
    private exportFile: ExportFile;
    private referencePanel: FFTReferencePanel;
    private client: Client;
    private stopped = false;

    constructor(private window: Window, private document: Document, ip: string,
                private options: FFTWorkspaceOptions) {
        this.client = new Client(ip, 5);
        window.addEventListener('HTMLImportsLoaded', async () => {
            try {
                new Imports(document);
                document.getElementById('board-label').textContent = options.boardName;
                if (options.instrumentName) { document.querySelector('h1').textContent = options.instrumentName; }
                if (options.signalGenerator === false) {
                    document.querySelector<HTMLElement>('.fft-generator').hidden = true;
                }
                await this.client.init();
                if (this.stopped) { return; }
                this.fft = options.createDriver(this.client);
                await this.fft.init();
                if (this.stopped) { return; }
                this.board = options.createBoard(this.client, document, this.fft);
                this.fftApp = new FFTApp(document, this.fft,
                    rate => this.generator?.setSampleRate(rate),
                    values => this.board.precisionDacChanged?.(values));
                // Imports mount the shared plot before selecting its placeholder.
                const placeholder = $('#plot-placeholder');
                const grid = this.fft.status.spectrum;
                const plotBasics = new PlotBasics(document, placeholder, grid ? grid.frequencies.length : this.fft.fft_size / 2,
                    grid ? 10 : 0, grid ? this.fft.status.fs / 2 : this.fft.status.fs / 1e6 / 2,
                    -200, 170, this.fft, '', 'Frequency (' + (grid ? grid.unit : 'MHz') + ')');
                if (grid?.logarithmic) { plotBasics.setLogX(true); }
                this.plot = new Plot(document, this.fft, plotBasics);
                this.referencePanel = new FFTReferencePanel(document, this.plot.references, () => this.plot.refreshReferences(), item => this.plot.replaceReference(item));
                await this.board.init();
                if (this.stopped) { return; }
                this.exportFile = new ExportFile(document, this.plot, options.boardName, options.instrumentName || 'FFT');
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
                reset.addEventListener('click', () => placeholder.trigger('dblclick'));
                if (options.signalGenerator === false) { return; }
                this.generator = new PhaseModulatorWidget(document.getElementById('phase-modulator'),
                    new PhaseModulatorDriver(this.client), {expectedChannels: 2, halfScaleOutput: options.halfScaleOutput});
                void this.generator.init().then(() => {
                    if (!this.stopped) { this.generator.setSampleRate(this.fft.status.fs); }
                }).catch(() => {});
            } catch (error) {
                if (this.stopped) { return; }
                document.getElementById('connection-error').hidden = false;
                const status = document.getElementById('connection-status');
                status.textContent = 'Disconnected';
                status.dataset.state = 'error';
                console.error('FFT initialization failed:', error);
                this.shutdown();
            }
        });
        window.addEventListener('pagehide', () => this.shutdown());
        window.addEventListener('beforeunload', () => this.shutdown());
        window.addEventListener('pageshow', event => {
            if (event.persisted) { window.location.reload(); }
        });
    }

    private shutdown(): void {
        if (this.stopped) { return; }
        this.stopped = true;
        this.referencePanel?.dispose();
        this.plot?.dispose();
        this.fftApp?.dispose();
        this.generator?.dispose();
        this.board?.dispose();
        for (const id of ['instrument-controls', 'board-controls']) {
            (this.document.getElementById(id) as HTMLFieldSetElement).disabled = true;
        }
        for (const id of ['pause-display', 'reset-view', 'spectrum-view']) {
            (this.document.getElementById(id) as HTMLButtonElement | HTMLSelectElement).disabled = true;
        }
        this.client.exit();
    }
}
