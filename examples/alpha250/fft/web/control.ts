// Control widget
// (c) Koheron

class Control {
    private channelNum: number = 2;
    private fftChanelInputs: HTMLInputElement[];

    private precisionDacNum: number = 4;
    private precisionDacInputs: HTMLInputElement[];

    private clkgenInputs: HTMLInputElement[];
    private fftSelects: HTMLSelectElement[];
    private fftInputs: HTMLInputElement[];

    constructor(document: Document, private fft: FFT, private PrecisionDac: PrecisionDac, private clkGen: ClockGenerator) {
        this.fftChanelInputs = <HTMLInputElement[]><any>document.getElementsByClassName("fft-channel-input");
        this.initFFTChannelInputs();

        this.precisionDacInputs = <HTMLInputElement[]><any>document.getElementsByClassName("precision-dac-input");
        this.initPrecisionDacInputs();

        this.clkgenInputs = <HTMLInputElement[]><any>document.getElementsByClassName("clkgen-input");
        this.initClkgenInputs();

        this.fftSelects = <HTMLSelectElement[]><any>document.getElementsByClassName("fft-select");
        this.initFFTSelects();
        this.fftInputs = <HTMLInputElement[]><any>document.getElementsByClassName("fft-input");
        this.initFFTInputs();

        this.updateDacValues();
        this.updateReferenceClock();
        this.updateFFTWindowInputs();
        this.updateControls();
    }

    // Updateters

    private updateControls() {
        this.fft.getControlParameters( (sts: IFFTStatus) => {

            for (let i = 0; i < this.channelNum; i++) {
                let inputs = <HTMLInputElement[]><any>document.querySelectorAll(".fft-channel-input[data-command='setDDSFreq'][data-channel='" + i.toString() + "']");
                let inputsArray = [];
                for (let j = 0; j < inputs.length; j++) {
                    inputsArray.push(inputs[j]);
                }

                if (inputsArray.indexOf(<HTMLInputElement>document.activeElement) == -1) {
                    for (let j = 0; j < inputs.length; j++) {
                      inputs[j].value = (sts.dds_freq[i] / 1e6).toFixed(6);
                      if (inputs[j].type == "range") {
                        inputs[j].max = (sts.fs / 1e6 / 2).toFixed(1);
                      }
                    }
                }
            }

            if (sts.fs === 200E6) {
                (<HTMLInputElement>document.querySelector("[data-command='setSamplingFrequency'][value='0']")).checked = true;
            } else {
                (<HTMLInputElement>document.querySelector("[data-command='setSamplingFrequency'][value='1']")).checked = true;
            }

            (<HTMLInputElement>document.querySelector("[data-command='setInputChannel'][value='" + sts.channel.toString() + "']")).checked = true;

            requestAnimationFrame( () => { this.updateControls(); } )
        });
    }

    private updateDacValues() {
        this.PrecisionDac.getDacValues( (dacValues: Float32Array) => {
            for (let i = 0; i < this.precisionDacNum; i++) {
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

            requestAnimationFrame( () => { this.updateDacValues(); } )
        });
    }

    private updateReferenceClock() {
        this.clkGen.getReferenceClock( (clkin: number) => {

            let clkIndex: string = "0";
            if (clkin !== 0) {
                clkIndex = "2";
            }

            (<HTMLInputElement>document.querySelector("[data-command='setReferenceClock'][value='" + clkIndex + "']")).checked = true;
            requestAnimationFrame( () => { this.updateReferenceClock(); } )
        });
    }

    private updateFFTWindowInputs() {
        this.fft.getFFTWindowIndex( (windowIndex: number) => {
            (<HTMLSelectElement>document.querySelector("[data-command='setFFTWindow']")).value = windowIndex.toString();
            requestAnimationFrame( () => { this.updateFFTWindowInputs(); } )
        });
    }

    // Setters

    initFFTChannelInputs(): void {
        let events = ['change', 'input'];
        for (let j = 0; j < events.length; j++) {
            for (let i = 0; i < this.fftChanelInputs.length; i++) {
                this.fftChanelInputs[i].addEventListener(events[j], (event) => {
                    let counterType: string = "number";
                    if ((<HTMLInputElement>event.currentTarget).type == "number") {
                        counterType = "range";
                    }
                    let command = (<HTMLInputElement>event.currentTarget).dataset.command;
                    let channel = (<HTMLInputElement>event.currentTarget).dataset.channel;
                    let value = (<HTMLInputElement>event.currentTarget).value;
                    (<HTMLInputElement>document.querySelector("[data-command='" + command + "'][data-channel='" + channel +"'][type='" + counterType + "']")).value = value ;
                    this.fft[command](channel, 1e6 * parseFloat(value));
                })
            }
        }

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
                    this.PrecisionDac[command](channel, parseFloat(value) / 1000);
                })
            }
        }
    }

    initFFTSelects(): void {
        for (let i = 0; i < this.fftSelects.length; i++) {
            this.fftSelects[i].addEventListener('change', (event) => {
                this.fft[(<HTMLSelectElement>event.currentTarget).dataset.command]((<HTMLSelectElement>event.currentTarget).value);
            })
        }
    }

    initFFTInputs(): void {
        for (let i = 0; i < this.fftInputs.length; i++) {
            this.fftInputs[i].addEventListener('change', (event) => {
                this.fft[(<HTMLInputElement>event.currentTarget).dataset.command]((<HTMLInputElement>event.currentTarget).value);
            })
        }
    }

    initClkgenInputs(): void {
        for (let i = 0; i < this.clkgenInputs.length; i ++) {
            this.clkgenInputs[i].addEventListener('change', (event) => {
                this.clkGen[(<HTMLInputElement>event.currentTarget).dataset.command](parseInt((<HTMLInputElement>event.currentTarget).value));
            })
        }
    }
}