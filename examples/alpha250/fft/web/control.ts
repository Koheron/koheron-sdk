// Control widget
// (c) Koheron

class Control {
    private channelNum: number;

    private precisionDacNum: number;
    private precisionDacInputs: HTMLInputElement[];
    private precisionDacSliders: HTMLInputElement[];

    private frequencies: Array<number>;
    private frequencyInputs: HTMLInputElement[];
    private frequencySliders: HTMLInputElement[];

    private clkgenInputs: HTMLInputElement[];
    private fftSelects: HTMLSelectElement[];
    private fftInputs: HTMLInputElement[];

    constructor(document: Document, private fft: FFT, private PrecisionDac: PrecisionDac, private clkGen: ClockGenerator) {
        this.channelNum = 2;

        this.frequencyInputs = [];
        this.frequencySliders = [];

        for (let i: number = 0; i < this.channelNum; i++) {
            this.frequencyInputs[i] = <HTMLInputElement>document.getElementById('frequency-input-' + i.toString());
            this.frequencySliders[i] = <HTMLInputElement>document.getElementById('frequency-slider-' + i.toString());
        }

        this.frequencies = new Array(this.channelNum);

        this.precisionDacNum = 4;

        this.precisionDacInputs = [];
        this.precisionDacSliders = [];

        for (let i: number = 0; i < this.precisionDacNum; i++) {
            this.precisionDacInputs[i] = <HTMLInputElement>document.getElementById('precision-dac-input-' + i.toString());
            this.precisionDacSliders[i] = <HTMLInputElement>document.getElementById('precision-dac-slider-' + i.toString());
        }

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
            for (let i: number = 0; i < this.channelNum; i++) {
                if (document.activeElement !== this.frequencyInputs[i]) {
                    this.frequencyInputs[i].value = (sts.dds_freq[i] / 1e6).toFixed(6);
                }

                if (document.activeElement !== this.frequencySliders[i]) {
                    this.frequencySliders[i].value = (sts.dds_freq[i] / 1e6).toFixed(6);
                    this.frequencySliders[i].max = (sts.fs / 1e6 / 2).toFixed(1);
                }

                if (sts.fs === 200E6) {
                    (<HTMLInputElement>document.querySelector("[data-command='setSamplingFrequency'][value='0']")).checked = true;
                } else {
                    (<HTMLInputElement>document.querySelector("[data-command='setSamplingFrequency'][value='1']")).checked = true;
                }

                (<HTMLInputElement>document.querySelector("[data-command='setInputChannel'][value='" + sts.channel.toString() + "']")).checked = true;

            }

            requestAnimationFrame( () => { this.updateControls(); } )
        });
    }

    private updateDacValues() {
        this.PrecisionDac.getDacValues( (dacValues: Float32Array) => {
            for (let i: number = 0; i < this.precisionDacNum; i++) {
                if (document.activeElement !== this.precisionDacInputs[i]) {
                    this.precisionDacInputs[i].value = (dacValues[i] * 1000).toFixed(3).toString();
                }

                if (document.activeElement !== this.precisionDacSliders[i]) {
                    this.precisionDacSliders[i].value = (dacValues[i] * 1000).toFixed(3).toString();
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

    setFrequency(channel: number, input: HTMLInputElement) {
        let frequencyValue = input.value;

        if (input.type === 'number') {
            this.frequencySliders[channel].value = frequencyValue;
        } else if (input.type === 'range') {
            this.frequencyInputs[channel].value = frequencyValue;
        }

        this.fft.setDDSFreq(channel, 1e6 * parseFloat(frequencyValue));
    }

    setPrecisionDac(channel: number, input: HTMLInputElement) {
        let precisionDacValue = input.value;

        if (input.type === 'number') {
            this.precisionDacSliders[channel].value = precisionDacValue;
        } else if (input.type === 'range') {
            this.precisionDacInputs[channel].value = precisionDacValue;
        }

        this.PrecisionDac.setDac(channel, parseFloat(precisionDacValue) / 1000);
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