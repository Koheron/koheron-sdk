class App {
    public plot: Plot;

    private fft: FFT;
    public fftApp: FFTApp;

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

    private navigation: Navigation;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 10;
        let client = new Client(ip, sockpoolSize);


        window.addEventListener('load', () => {
            client.init( () => {
                this.fft = new FFT(client);
                this.precisionDac = new PrecisionDac(client);
                this.precisionAdc = new PrecisionAdc(client);
                this.temperatureSensor = new TemperatureSensor(client);
                this.powerMonitor = new PowerMonitor(client);
                this.clockGenerator = new ClockGenerator(client);
                this.navigation = new Navigation(document);

                this.fft.init( () => {
                    this.fftApp = new FFTApp(document, this.fft);
                    this.plot = new Plot(document, plot_placeholder, this.fft);
                    this.temperatureSensorApp = new TemperatureSensorApp(document, this.temperatureSensor);
                    this.powerMonitorApp = new PowerMonitorApp(document, this.powerMonitor);
                    this.clockGeneratorApp = new ClockGeneratorApp(document, this.clockGenerator);
                    this.precisionChannelsApp = new PrecisionChannelsApp(document, this.precisionAdc, this.precisionDac);
                    this.exportFile = new ExportFile(document, this.fft, this.plot);

                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));