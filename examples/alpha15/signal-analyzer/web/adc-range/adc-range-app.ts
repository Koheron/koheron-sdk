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

    private updateControls() {
        this.driver.inputRange(0, (ch0_range: number) => {
            (<HTMLInputElement>document.querySelector("[data-command='setInputRange'][value='" + ch0_range.toString() + "']")).checked = true;
    
            this.driver.inputRange(1, (ch1_range: number) => {
                (<HTMLInputElement>document.querySelector("[data-command='setInputRange'][value='" + (2 + ch1_range).toString() + "']")).checked = true;
                requestAnimationFrame( () => { this.updateControls(); } )
            });
        });
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