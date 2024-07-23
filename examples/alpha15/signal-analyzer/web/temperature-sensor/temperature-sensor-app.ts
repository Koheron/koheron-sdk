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
