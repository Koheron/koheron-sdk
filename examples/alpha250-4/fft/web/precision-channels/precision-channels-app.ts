class PrecisionChannelsApp {

    private precisionChannelsNum: number = 4;
    private precisionDacInputs: HTMLInputElement[];

    constructor(document: Document, private precisionAdc: PrecisionAdc, private precisionDac: PrecisionDac) {
        this.precisionDacInputs = <HTMLInputElement[]><any>document.getElementsByClassName("precision-dac-input");
        this.updatePrecisionAdc();
        this.updatePrecisionDac();
        this.initPrecisionDacInputs();
    }

    private updatePrecisionAdc() {
        this.precisionAdc.getAdcValues((adcValues: Float32Array) => {
            for (let i: number = 0; i < this.precisionChannelsNum; i++) {
                (<HTMLSpanElement>document.querySelector(".precision-adc-span[data-channel='" + i.toString() + "']")).textContent = (adcValues[i] * 1000).toFixed(4);
            }
            requestAnimationFrame( () => { this.updatePrecisionAdc(); });
        });
    }

    private updatePrecisionDac() {
        this.precisionDac.getDacValues( (dacValues: Float32Array) => {
            for (let i = 0; i < this.precisionChannelsNum; i++) {
                let inputs = <HTMLInputElement[]><any>document.querySelectorAll(".precision-dac-input[data-command='setDac'][data-channel='" + i.toString() + "']");
                let inputsArray = [];
                for (let j = 0; j < inputs.length; j++) {
                    inputsArray.push(inputs[j]);
                }

                if (inputsArray.indexOf(<HTMLInputElement>document.activeElement) == -1) {
                    for (let j = 0; j < inputs.length; j++) {
                      inputs[j].value = (dacValues[i] * 1000).toFixed(3).toString();
                    }
                }
            }

            requestAnimationFrame( () => { this.updatePrecisionDac(); } )
        });
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
