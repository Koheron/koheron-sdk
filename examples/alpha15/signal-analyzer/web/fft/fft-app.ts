// FFT widget
// (c) Koheron

class FFTApp {
    // private channelNum: number = 2;
    private fftSelects: HTMLSelectElement[];
    private fftInputs: HTMLInputElement[];

    constructor(document: Document, private fft: FFT, private decimator: Decimator) {
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
            (<HTMLInputElement>document.querySelector("[data-command='setInputChannel'][value='" + sts.channel.toString() + "']")).checked = true;
            window.setTimeout(() => this.updateControls(), 250)
        });
    }

    private updateFFTWindowInputs() {
        this.fft.getFFTWindowIndex( (windowIndex: number) => {
            const select = <HTMLSelectElement>document.querySelector("[data-command='setFFTWindow']");
            const value = windowIndex.toString();
            // Assigning even the same value resets a native dropdown's pending
            // selection. Leave it alone while the user is interacting with it.
            if (document.activeElement !== select && select.value !== value) {
                select.value = value;
            }
            window.setTimeout(() => this.updateFFTWindowInputs(), 250);
        });
    }

    // Setters

    initFFTSelects(): void {
        for (let i = 0; i < this.fftSelects.length; i++) {
            this.fftSelects[i].addEventListener('change', (event) => {
                this.fft[(<HTMLSelectElement>event.currentTarget).dataset.command]((<HTMLSelectElement>event.currentTarget).value);
                this.decimator[(<HTMLSelectElement>event.currentTarget).dataset.command]((<HTMLSelectElement>event.currentTarget).value);
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