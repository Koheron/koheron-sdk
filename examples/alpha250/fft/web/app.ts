class App {
    public control: Control;
    public plot: Plot;
    private fft: FFT;
    private precisionDac: PrecisionDac;

    private temperatureSensor: TemperatureSensor;
    private temperatureSensorApp: TemperatureSensorApp;

    private powerMonitor: PowerMonitor;
    private powerMonitorApp: PowerMonitorApp;

    private precisionAdc: PrecisionAdc;
    private clkGenerator: ClockGenerator;

    private precisionAdcNum: number = 4;

    private navigation: Navigation;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 10;
        let client = new Client(ip, sockpoolSize);


        window.addEventListener('load', () => {
            client.init( () => {
                this.fft = new FFT(client);
                this.precisionDac = new PrecisionDac(client);
                this.temperatureSensor = new TemperatureSensor(client);
                this.powerMonitor = new PowerMonitor(client);
                this.precisionAdc = new PrecisionAdc(client);
                this.clkGenerator = new ClockGenerator(client);
                this.navigation = new Navigation(document);

                this.fft.init( () => {
                    this.control = new Control(document, this.fft, this.precisionDac, this.clkGenerator);
                    this.plot = new Plot(document, plot_placeholder, this.fft);
                    this.temperatureSensorApp = new TemperatureSensorApp(document, this.temperatureSensor);
                    this.powerMonitorApp = new PowerMonitorApp(document, this.powerMonitor);
                    this.updatePrecisionAdcValues();
                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }


    private updatePrecisionAdcValues() {
        this.precisionAdc.getAdcValues((adcValues: Float32Array) => {
            for (let i: number = 0; i < this.precisionAdcNum; i++) {
                (<HTMLSpanElement>document.querySelector(".precision-adc-span[data-channel='" + i.toString() + "']")).textContent = (adcValues[i] * 1000).toFixed(4);
            }
            requestAnimationFrame( () => { this.updatePrecisionAdcValues(); });
        });
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));