class TemperatureSensor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('TemperatureSensor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getTemperatures(cb: (temperatures: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['get_temperatures']), (temperatures: Float32Array) => {cb(temperatures)});
    }
}

class TemperatureSensorApp {

    private temperatureSpans: HTMLSpanElement[];

    constructor(document: Document, private temperatureSensor: TemperatureSensor) {
        this.temperatureSpans = <HTMLSpanElement[]><any>document.getElementsByClassName("temperature-span");
        this.updateTemperatures();
    }

    private updateTemperatures() {
        this.temperatureSensor.getTemperatures((temperatures: Float32Array) => {
            for (let i = 0; i < this.temperatureSpans.length; i ++) {
                this.temperatureSpans[i].textContent = temperatures[parseInt(this.temperatureSpans[i].dataset.index)].toFixed(3);
            }
            requestAnimationFrame( () => { this.updateTemperatures(); } );
        });
    }
}
