// FFT widget
// (c) Koheron

class AdcRangeApp {
    private adcRanges: HTMLInputElement[];

    constructor(document: Document, private driver) {
        this.adcRanges = <HTMLInputElement[]><any>document.getElementsByClassName("adc-range");
        this.initFFTRanges();
        this.updateControls();
    }

    // Updaters

    private async updateControls() {
        const ch0_range = await this.driver.inputRange(0);
        const ch1_range = await this.driver.inputRange(1);
        (<HTMLInputElement>document.querySelector("[data-command='setInputRange'][value='" + ch0_range.toString() + "']")).checked = true;
        (<HTMLInputElement>document.querySelector("[data-command='setInputRange'][value='" + (2 + ch1_range).toString() + "']")).checked = true;
        requestAnimationFrame( () => { this.updateControls(); } )
    }

    // Setters

    initFFTRanges(): void {
        for (let i = 0; i < this.adcRanges.length; i++) {
            this.adcRanges[i].addEventListener('change', (event) => {
                this.driver[(<HTMLInputElement>event.currentTarget).dataset.command]((<HTMLInputElement>event.currentTarget).value);
            })
        }
    }
}