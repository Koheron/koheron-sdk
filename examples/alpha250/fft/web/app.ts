class App {
    public control: Control;
    public plot: Plot;
    private fft: FFT;
    private precisionDac: PrecisionDac;

    private temperatureSensor: TemperatureSensor;
    private powerMonitor: PowerMonitor;
    private precisionAdc: PrecisionAdc;
    private clkGenerator: ClockGenerator;

    private temperatureVoltageReference: HTMLSpanElement;
    private temperatureBoardSpan: HTMLSpanElement;
    private temperatureZynqSpan: HTMLSpanElement;
    private supplyMainVoltageSpan: HTMLSpanElement;
    private supplyMainCurrentSpan: HTMLSpanElement;
    private supplyClockVoltageSpan: HTMLSpanElement;
    private supplyClockCurrentSpan: HTMLSpanElement;

    private precisionAdcNum: number;
    private precisionAdcSpans: HTMLSpanElement[];

    private navigation: Navigation;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 10;
        let client = new Client(ip, sockpoolSize);

        this.temperatureVoltageReference = <HTMLSpanElement>document.getElementById('temperature-voltage-reference');
        this.temperatureBoardSpan = <HTMLSpanElement>document.getElementById('temperature-board');
        this.temperatureZynqSpan = <HTMLSpanElement>document.getElementById('temperature-zynq');
        this.supplyMainVoltageSpan = <HTMLSpanElement>document.getElementById('supply-main-voltage');
        this.supplyMainCurrentSpan = <HTMLSpanElement>document.getElementById('supply-main-current');
        this.supplyClockVoltageSpan = <HTMLSpanElement>document.getElementById('supply-clock-voltage');
        this.supplyClockCurrentSpan = <HTMLSpanElement>document.getElementById('supply-clock-current');

        this.precisionAdcNum = 4;
        this.precisionAdcSpans = [];

        for (let i:number = 0; i < this.precisionAdcNum; i++) {
            this.precisionAdcSpans[i] = <HTMLInputElement>document.getElementById('precision-adc-' + i.toString());
        }

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
                    this.updateTemperatures();
                    this.updateSupplies();
                    this.updatePrecisionAdcValues();
                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

    private updateTemperatures() {
        this.temperatureSensor.getTemperatures((temperatures: Float32Array) => {
            this.temperatureVoltageReference.innerHTML = temperatures[0].toFixed(3).toString();
            this.temperatureBoardSpan.innerHTML = temperatures[1].toFixed(3).toString();
            this.temperatureZynqSpan.innerHTML = temperatures[2].toFixed(3).toString();

            requestAnimationFrame( () => { this.updateTemperatures(); } );
        });
    }

    private updateSupplies() {
        this.powerMonitor.getSuppliesUI((supplyValues: Float32Array) => {
            this.supplyMainCurrentSpan.innerHTML = (supplyValues[0] * 1E3).toFixed(1).toString();
            this.supplyMainVoltageSpan.innerHTML = supplyValues[1].toFixed(3).toString();
            this.supplyClockCurrentSpan.innerHTML = (supplyValues[2] * 1E3).toFixed(1).toString();
            this.supplyClockVoltageSpan.innerHTML = supplyValues[3].toFixed(3).toString();

            requestAnimationFrame( () => { this.updateSupplies(); });
        });
    }

    private updatePrecisionAdcValues() {
        this.precisionAdc.getAdcValues((adcValues: Float32Array) => {
            for (let i: number = 0; i < this.precisionAdcNum; i++) {
                this.precisionAdcSpans[i].innerHTML = (adcValues[i] * 1000).toFixed(4).toString();
            }

            requestAnimationFrame( () => { this.updatePrecisionAdcValues(); });
        });
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));