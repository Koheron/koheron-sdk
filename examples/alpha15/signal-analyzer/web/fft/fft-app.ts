// FFT widget
// (c) Koheron

class FFTApp {
    private windowSelect: HTMLSelectElement;
    private fftInputs: HTMLInputElement[];
    private windowRevision = 0;
    private pendingWindow: number = null;
    private windowDeadline = 0;

    constructor(private document: Document, private fft: FFT, private decimator: Decimator) {
        this.windowSelect = <HTMLSelectElement>document.getElementById("window");
        this.windowSelect.addEventListener("change", () => {
            const index = Number(this.windowSelect.value);
            this.windowRevision++;
            this.pendingWindow = index;
            this.windowDeadline = performance.now() + 3000;
            document.getElementById("window-status").textContent = "Applying…";
            this.fft.setFFTWindow(index);
            this.decimator.setFFTWindow(index);
        });
        this.windowSelect.addEventListener("blur", () => {
            if (this.pendingWindow === null) {
                this.showWindow(this.fft.windowIndex);
            }
        });
        this.fftInputs = <HTMLInputElement[]><any>document.getElementsByClassName("fft-input");
        this.initFFTInputs();
        this.updateFFTWindowInputs();
        this.updateControls();
    }

    private showWindow(index: number): void {
        this.fft.windowIndex = index;
        if (this.windowSelect.disabled) {
            this.windowSelect.disabled = false;
        }
        // Native dropdowns have an uncommitted selection while open. Do not
        // assign their value while focused, even if the server value matches.
        const value = String(index);
        if (this.document.activeElement !== this.windowSelect && this.windowSelect.value !== value) {
            this.windowSelect.value = value;
        }
    }

    // Updaters

    private updateControls() {
        this.fft.getControlParameters( (sts: IFFTStatus) => {
            (<HTMLInputElement>document.querySelector("[data-command='setInputChannel'][value='" + sts.channel.toString() + "']")).checked = true;
            window.setTimeout(() => this.updateControls(), 250)
        });
    }

    private updateFFTWindowInputs() {
        const revision = this.windowRevision;
        this.fft.getFFTWindowIndex((index: number) => {
            if (revision === this.windowRevision) {
                const pending = this.pendingWindow !== null;
                if (!pending || index === this.pendingWindow || performance.now() >= this.windowDeadline) {
                    if (pending) {
                        this.document.getElementById("window-status").textContent = index === this.pendingWindow
                            ? "" : "Window change not confirmed. Please try again.";
                    }
                    this.pendingWindow = null;
                    this.showWindow(index);
                }
            }
            window.setTimeout(() => this.updateFFTWindowInputs(), 250);
        });
    }

    initFFTInputs(): void {
        for (let i = 0; i < this.fftInputs.length; i++) {
            this.fftInputs[i].addEventListener('change', (event) => {
                this.fft[(<HTMLInputElement>event.currentTarget).dataset.command]((<HTMLInputElement>event.currentTarget).value);
            })
        }
    }
}