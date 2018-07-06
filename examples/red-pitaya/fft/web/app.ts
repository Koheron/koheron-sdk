class App {

    public plot: Plot;
    private fft: FFT;
    public fftApp: FFTApp;
    public laserDriver: LaserDriver;
    public laserControl: LaserControl;
    private navigation: Navigation;
    private exportFile: ExportFile;
    private imports: Imports;
    public ddsFrequency: DDSFrequency;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let client = new Client(ip, 5);

        window.addEventListener('load', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.fft = new FFT(client);
                this.navigation = new Navigation(document);

                this.fft.init( () => {
                    this.fftApp = new FFTApp(document, this.fft);
                    this.ddsFrequency = new DDSFrequency(document, this.fft);
                    this.plot = new Plot(document, plot_placeholder, this.fft);
                    this.laserDriver = new LaserDriver(client);
                    this.laserControl = new LaserControl(document, this.laserDriver);
                    this.exportFile = new ExportFile(document, this.fft, this.plot);
                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));