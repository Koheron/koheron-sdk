class App {
    public control: Control;
    public plot: Plot;
    private fft: FFT;
    private precisionDac: PrecisionDac;

    private temperatureSensor: TemperatureSensor;
    private temperatureSensorApp: TemperatureSensorApp;

    private powerMonitor: PowerMonitor;
    private precisionAdc: PrecisionAdc;
    private clkGenerator: ClockGenerator;

    private supplySpans: HTMLSpanElement[];

    private precisionAdcNum: number = 4;

    private navigation: Navigation;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 10;
        let client = new Client(ip, sockpoolSize);

        this.supplySpans = <HTMLSpanElement[]><any>document.getElementsByClassName("supply-span");

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
                    this.updateSupplies();
                    this.updatePrecisionAdcValues();
                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

    private updateSupplies() {
        this.powerMonitor.getSuppliesUI((supplyValues: Float32Array) => {
            for (let i = 0; i < this.supplySpans.length; i ++) {
                let value: string = "";
                if (this.supplySpans[i].dataset.type === "voltage") {
                    value = supplyValues[parseInt(this.supplySpans[i].dataset.index)].toFixed(3);
                } else if (this.supplySpans[i].dataset.type === "current") {
                    value = (supplyValues[parseInt(this.supplySpans[i].dataset.index)] * 1E3).toFixed(1);
                }
                this.supplySpans[i].textContent = value;
            }
            requestAnimationFrame( () => { this.updateSupplies(); });
        });
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