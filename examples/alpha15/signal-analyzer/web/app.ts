class App {
    private imports: Imports;
    public plot: Plot;
    private plotBasics: PlotBasics;
    private fft: FFT;
    public fftApp: FFTApp;
    private decimator: Decimator;
    private temperatureSensor: TemperatureSensor;
    private temperatureSensorApp: TemperatureSensorApp;
    private powerMonitor: PowerMonitor;
    private powerMonitorApp: PowerMonitorApp;
    private clockGenerator: ClockGenerator;
    private clockGeneratorApp: ClockGeneratorApp;
    private precisionDac: PrecisionDac;
    private precisionChannelsApp: PrecisionChannelsApp;
    private ltc2387: Ltc2387;
    private adcRangeApp: AdcRangeApp;

    private n_pts: number;
    private x_min: number;
    private x_max: number;
    private y_min: number;
    private y_max: number;

    constructor(window: Window,
                document: Document,
                ip: string,
                plot_placeholder: JQuery) {
        let sockpoolSize: number = 10;
        let client = new Client(ip, sockpoolSize);

        window.addEventListener('HTMLImportsLoaded', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.fft = new FFT(client);
                this.decimator = new Decimator(client);
                this.precisionDac = new PrecisionDac(client);
                this.temperatureSensor = new TemperatureSensor(client);
                this.powerMonitor = new PowerMonitor(client);
                this.clockGenerator = new ClockGenerator(client);
                this.ltc2387 = new Ltc2387(client);

                this.fft.init( () => {
                    this.decimator.init( () => {
                        this.initApp(plot_placeholder);
                    });
                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

    private initApp(plot_placeholder: JQuery) {
        this.fftApp = new FFTApp(document, this.fft);

        // FFT plot
        this.n_pts = this.fft.fft_size / 2;
        this.x_min = 100;
        this.x_max = this.fft.status.fs / 1E6 / 2;
        this.y_min = -150;
        this.y_max = 50;

        this.plotBasics = new PlotBasics(document, plot_placeholder,
                                         this.n_pts,
                                         this.x_min, this.x_max,
                                         this.y_min, this.y_max,
                                         this.fft, "", "Frequency (Hz)");
        this.plot = new Plot(document, this.fft, this.decimator, this.plotBasics);

        this.temperatureSensorApp = new TemperatureSensorApp(document, this.temperatureSensor);
        this.powerMonitorApp = new PowerMonitorApp(document, this.powerMonitor);
        this.clockGeneratorApp = new ClockGeneratorApp(document, this.clockGenerator);
        this.precisionChannelsApp = new PrecisionChannelsApp(document, this.precisionDac);
        this.adcRangeApp = new AdcRangeApp(document, this.ltc2387);
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));