class App {

    private imports: Imports;
    public plot: Plot;
    private plotBasics: PlotBasics;
    private fft: FFT;
    public fftApp: FFTApp;
    public ddsFrequency: DDSFrequency;
    private temperatureSensor: TemperatureSensor;
    private temperatureSensorApp: TemperatureSensorApp;
    private powerMonitor: PowerMonitor;
    private powerMonitorApp: PowerMonitorApp;
    private clockGenerator: ClockGenerator;
    private clockGeneratorApp: ClockGeneratorApp;
    private precisionDac: PrecisionDac;
    private precisionAdc: PrecisionAdc;
    private precisionChannelsApp: PrecisionChannelsApp;
    private exportFile: ExportFile;

    private n_pts: number;
    private x_min: number;
    private x_max: number;
    private y_min: number;
    private y_max: number;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 10;
        let client = new Client(ip, sockpoolSize);

        window.addEventListener('HTMLImportsLoaded', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.fft = new FFT(client);
                this.precisionDac = new PrecisionDac(client);
                this.precisionAdc = new PrecisionAdc(client);
                this.temperatureSensor = new TemperatureSensor(client);
                this.powerMonitor = new PowerMonitor(client);
                this.clockGenerator = new ClockGenerator(client);

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

                    this.temperatureSensorApp = new TemperatureSensorApp(document, this.temperatureSensor);
                    this.powerMonitorApp = new PowerMonitorApp(document, this.powerMonitor);
                    this.clockGeneratorApp = new ClockGeneratorApp(document, this.clockGenerator);
                    this.precisionChannelsApp = new PrecisionChannelsApp(document, this.precisionAdc, this.precisionDac);
                    this.exportFile = new ExportFile(document, this.plot);

                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));