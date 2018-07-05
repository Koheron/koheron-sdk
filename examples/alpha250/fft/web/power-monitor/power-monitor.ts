class PowerMonitor {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('PowerMonitor');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getSuppliesUI(cb: (values: Float32Array) => void): void {
        this.client.readFloat32Array(Command(this.id, this.cmds['get_supplies_ui']),
                                 (values: Float32Array) => {cb(values)});
    }
}

class PowerMonitorApp {

    private supplySpans: HTMLSpanElement[];

    constructor(document: Document, private powerMonitor: PowerMonitor) {
        this.supplySpans = <HTMLSpanElement[]><any>document.getElementsByClassName("supply-span");
        this.updateSupplies();
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
}