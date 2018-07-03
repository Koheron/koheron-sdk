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

    public referenceClock: string;
    private referenceClockInternalInput: HTMLInputElement;
    private referenceClockExternalInput: HTMLInputElement;

    private samplingFrequency: string;
    private samplingFrequency200Input: HTMLInputElement;
    private samplingFrequency250Input: HTMLInputElement;

    public fftWindowIndex: number;
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

        this.referenceClock = 'internal';
        this.referenceClockInternalInput = <HTMLInputElement>document.getElementById('reference-clock-internal');
        this.referenceClockExternalInput = <HTMLInputElement>document.getElementById('reference-clock-external');

        this.samplingFrequency = '250 MHz';
        this.samplingFrequency200Input = <HTMLInputElement>document.getElementById('sampling-frequency-200');
        this.samplingFrequency250Input = <HTMLInputElement>document.getElementById('sampling-frequency-250');

        this.fftWindowIndex = 1;
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
                    this.samplingFrequency = '200 MHz';
                    this.samplingFrequency200Input.checked = true;
                } else {
                    this.samplingFrequency = '250 MHz';
                    this.samplingFrequency250Input.checked = true;
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
            if (clkin === 0) {
                this.referenceClock = 'external';
                this.referenceClockExternalInput.checked = true;
            } else {
                this.referenceClock = 'internal';
                this.referenceClockInternalInput.checked = true;
            }

            requestAnimationFrame( () => { this.updateReferenceClock(); } )
        });
    }

    private updateFFTWindowInputs() {
        this.fft.getFFTWindowIndex( (windowIndex: number) => {
            this.fftWindowIndex = windowIndex;
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

    setReferenceClock(referenceClock: string) {
        this.referenceClock = referenceClock;

        if (this.referenceClock === 'external') {
            this.clkGen.setReferenceClock(0);
        } else {
            this.clkGen.setReferenceClock(2);
        }
    }

    setSamplingFrequency(samplingFrequency: string) {
        this.samplingFrequency = samplingFrequency;

        if (this.samplingFrequency === '200 MHz') {
            this.clkGen.setSamplingFrequency(0);
        } else { // 250 MHz
            this.clkGen.setSamplingFrequency(1);
        }
    }

    setInputChannel(channel: number) {
        this.fft.setInputChannel(channel);
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
}