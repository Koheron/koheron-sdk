class App {
    public control: Control;
    public plot: Plot;
    private driver: DDS;

    private tempSensor: TemperatureSensor;
    private powMonitor: PowerMonitor;

    private temp0Span: HTMLSpanElement;
    private temp1Span: HTMLSpanElement;
    private temp2Span: HTMLSpanElement;
    private supplyVoltage0Span: HTMLSpanElement;
    private supplyCurrent0Span: HTMLSpanElement;
    private supplyVoltage1Span: HTMLSpanElement;
    private supplyCurrent1Span: HTMLSpanElement;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let client = new Client(ip, 5);

        this.temp0Span = <HTMLSpanElement>document.getElementById('temp0');
        this.temp1Span = <HTMLSpanElement>document.getElementById('temp1');
        this.temp2Span = <HTMLSpanElement>document.getElementById('temp2');
        this.supplyVoltage0Span = <HTMLSpanElement>document.getElementById('supply_voltage0');
        this.supplyCurrent0Span = <HTMLSpanElement>document.getElementById('supply_current0');
        this.supplyVoltage1Span = <HTMLSpanElement>document.getElementById('supply_voltage1');
        this.supplyCurrent1Span = <HTMLSpanElement>document.getElementById('supply_current1');

        window.addEventListener('load', () => {
            client.init( () => {
                this.driver = new DDS(client);
                this.control = new Control(document, this.driver);
                this.plot = new Plot(document, plot_placeholder, this.driver);
                this.tempSensor = new TemperatureSensor(client);
                this.powMonitor = new PowerMonitor(client);
                this.acquireData();
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

    acquireData() {
        this.tempSensor.getTemperature(0, (temp: number) => {
            this.temp0Span.innerHTML = temp.toFixed(3).toString();

            this.tempSensor.getTemperature(1, (temp: number) => {
                this.temp1Span.innerHTML = temp.toFixed(3).toString();

                this.tempSensor.getTemperature(2, (temp: number) => {
                    this.temp2Span.innerHTML = temp.toFixed(3).toString();

                    this.powMonitor.getSupplyCurrent(0, (curr: number) => {
                        this.supplyCurrent0Span.innerHTML = (curr * 1E3).toFixed(1).toString();

                        this.powMonitor.getBusVoltage(0, (volt: number) => {
                            this.supplyVoltage0Span.innerHTML = volt.toFixed(3).toString();

                            this.powMonitor.getSupplyCurrent(1, (curr: number) => {
                                this.supplyCurrent1Span.innerHTML = (curr * 1E3).toFixed(1).toString();

                                this.powMonitor.getBusVoltage(1, (volt: number) => {
                                    this.supplyVoltage1Span.innerHTML = volt.toFixed(3).toString();

                                    requestAnimationFrame( () => {
                                        this.acquireData();
                                    });
                                });
                            });
                        });
                    });
                });
            });
        });
    }

}



let app = new App(window, document, location.hostname, $('#plot-placeholder'));