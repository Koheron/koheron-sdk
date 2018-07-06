// FFT widget
// (c) Koheron

class FFTApp {
    private channelNum: number = 2;
    private fftSelects: HTMLSelectElement[];
    private fftInputs: HTMLInputElement[];

    constructor(document: Document, private fft: FFT) {
        this.fftSelects = <HTMLSelectElement[]><any>document.getElementsByClassName("fft-select");
        this.initFFTSelects();
        this.fftInputs = <HTMLInputElement[]><any>document.getElementsByClassName("fft-input");
        this.initFFTInputs();

        this.updateFFTWindowInputs();
        this.updateControls();
    }

    // Updaters

    private updateControls() {
        this.fft.getControlParameters( (sts: IFFTStatus) => {

            for (let i = 0; i < this.channelNum; i++) {
                let inputs = <HTMLInputElement[]><any>document.querySelectorAll(".dds-channel-input[data-command='setDDSFreq'][data-channel='" + i.toString() + "']");
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

    private updateFFTWindowInputs() {
        this.fft.getFFTWindowIndex( (windowIndex: number) => {
            (<HTMLSelectElement>document.querySelector("[data-command='setFFTWindow']")).value = windowIndex.toString();
            requestAnimationFrame( () => { this.updateFFTWindowInputs(); } )
        });
    }

    // Setters

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

}