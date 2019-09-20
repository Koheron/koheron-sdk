// FFT widget
// (c) Koheron

class FFTApp {
    private channelNum: number = 2;
    private fftSelects: HTMLSelectElement[];
    private fftInputs: HTMLInputElement[];

    constructor(document: Document, private driver) {
        this.fftSelects = <HTMLSelectElement[]><any>document.getElementsByClassName("fft-select");
        this.initFFTSelects();
        this.fftInputs = <HTMLInputElement[]><any>document.getElementsByClassName("fft-input");
        this.initFFTInputs();

        this.updateFFTWindowInputs();
        this.updateControls();
    }

    // Updaters

    private updateControls() {
        this.driver.getControlParameters( (sts: IFFTStatus) => {
            if (sts.fs[0] === 200E6) {
                (<HTMLInputElement>document.querySelector("[data-command='setSamplingFrequency'][value='0']")).checked = true;
            } else {
                (<HTMLInputElement>document.querySelector("[data-command='setSamplingFrequency'][value='1']")).checked = true;
            }

            (<HTMLInputElement>document.querySelector("[data-command='setInputChannel'][value='" + sts.channel.toString() + "']")).checked = true;
            requestAnimationFrame( () => { this.updateControls(); } )
        });
    }

    private updateFFTWindowInputs() {
        this.driver.getFFTWindowIndex( (windowIndex: number) => {
            (<HTMLSelectElement>document.querySelector("[data-command='setFFTWindow']")).value = windowIndex.toString();
            requestAnimationFrame( () => { this.updateFFTWindowInputs(); } )
        });
    }

    // Setters

    initFFTSelects(): void {
        for (let i = 0; i < this.fftSelects.length; i++) {
            this.fftSelects[i].addEventListener('change', (event) => {
                this.driver[(<HTMLSelectElement>event.currentTarget).dataset.command]((<HTMLSelectElement>event.currentTarget).value);
            })
        }
    }

    initFFTInputs(): void {
        for (let i = 0; i < this.fftInputs.length; i++) {
            this.fftInputs[i].addEventListener('change', (event) => {
                this.driver[(<HTMLInputElement>event.currentTarget).dataset.command]((<HTMLInputElement>event.currentTarget).value);
            })
        }
    }

}