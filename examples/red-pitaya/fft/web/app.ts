class App {

    public plot: Plot;
    private fft: FFT;
    public fftApp: FFTApp;
    public laserDriver: LaserDriver;
    public laserControl: LaserControl;
    private exportFile: ExportFile;
    private imports: Imports;
    public ddsFrequency: DDSFrequency;
    private plotBasics: PlotBasics;

    private n_pts: number;
    private x_min: number;
    private x_max: number;
    private y_min: number;
    private y_max: number;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let client = new Client(ip, 5);

        window.addEventListener('HTMLImportsLoaded', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.fft = new FFT(client);

                this.fft.init( () => {
                    this.fftApp = new FFTApp(document, this.fft);
                    this.ddsFrequency = new DDSFrequency(document, this.fft);

                    this.n_pts = this.fft.fft_size / 2;
                    this.x_min = 0;
                    this.x_max = this.fft.status.fs / 1E6 / 2;
                    this.y_min = -200;
                    this.y_max = 170;

                    this.plotBasics = new PlotBasics(document, plot_placeholder, this.plot, this.n_pts, this.x_min, this.x_max, this.y_min, this.y_max, this.fft, "", "Frequency (MHz)");
                    this.plot = new Plot(document, this.fft, this.plotBasics);

                    this.laserDriver = new LaserDriver(client);
                    this.laserControl = new LaserControl(document, this.laserDriver);
                    this.exportFile = new ExportFile(document, this.plot);
                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));