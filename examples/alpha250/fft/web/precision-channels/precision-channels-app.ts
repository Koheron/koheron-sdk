class PrecisionChannelsApp {
    private precisionDacInputs: HTMLInputElement[];

    constructor(document: Document, private precisionDac: PrecisionDac) {
        this.precisionDacInputs = <HTMLInputElement[]><any>document.getElementsByClassName("precision-dac-input");
        this.initPrecisionDacInputs();
    }

    initPrecisionDacInputs(): void {
        let events = ['change', 'input'];
        for (let j = 0; j < events.length; j++) {
            for (let i = 0; i < this.precisionDacInputs.length; i++) {
                this.precisionDacInputs[i].addEventListener(events[j], (event) => {
                    let counterType: string = "number";
                    if ((<HTMLInputElement>event.currentTarget).type == "number") {
                        counterType = "range";
                    }
                    let command = (<HTMLInputElement>event.currentTarget).dataset.command;
                    let channel = (<HTMLInputElement>event.currentTarget).dataset.channel;
                    let value = (<HTMLInputElement>event.currentTarget).value;
                    (<HTMLInputElement>document.querySelector("[data-command='" + command + "'][data-channel='" + channel +"'][type='" + counterType + "']")).value = value ;
                    this.precisionDac[command](channel, parseFloat(value) / 1000);
                })
            }
        }
    }
}
