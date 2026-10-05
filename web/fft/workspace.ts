interface FFTBoardControls {
    init(): Promise<void>;
    precisionDacChanged?(values: ArrayLike<number>): void;
    dispose(): void;
}

interface FFTWorkspaceOptions {
    boardName: string;
    halfScaleOutput?: boolean;
    createDriver: (client: Client) => FFTDriver;
    createBoard: (client: Client, document: Document) => FFTBoardControls;
}

// Both boards use this lifecycle, acquisition controls, plot, actions and exports.
class FFTWorkspace {
    public plot: Plot;
    public fftApp: FFTApp;
    private fft: FFTDriver;
    private generator: PhaseModulatorWidget;
    private plotBasics: PlotBasics;
    private board: FFTBoardControls;
    private exportFile: ExportFile;
    private client: Client;
    private stopped = false;

    constructor(private window: Window, private document: Document, ip: string,
                private options: FFTWorkspaceOptions) {
        this.client = new Client(ip, 5);
        window.addEventListener('HTMLImportsLoaded', async () => {
            try {
                new Imports(document);
                document.getElementById('board-label').textContent = options.boardName;
                await this.client.init();
                if (this.stopped) { return; }
                this.fft = options.createDriver(this.client);
                await this.fft.init();
                if (this.stopped) { return; }
                this.board = options.createBoard(this.client, document);
                this.fftApp = new FFTApp(document, this.fft,
                    rate => this.generator?.setSampleRate(rate),
                    values => this.board.precisionDacChanged?.(values));
                // Imports mount the shared plot before selecting its placeholder.
                const placeholder = $('#plot-placeholder');
                this.plotBasics = new PlotBasics(document, placeholder, this.fft.fft_size / 2,
                    0, this.fft.status.fs / 1e6 / 2, -200, 170, this.fft, '', 'Frequency (MHz)');
                this.plot = new Plot(document, this.fft, this.plotBasics);
                await this.board.init();
                if (this.stopped) { return; }
                this.exportFile = new ExportFile(document, this.plot, options.boardName);
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
        this.plot?.dispose();
        this.fftApp?.dispose();
        this.generator?.dispose();
        this.board?.dispose();
        this.client.exit();
    }
}
