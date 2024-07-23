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