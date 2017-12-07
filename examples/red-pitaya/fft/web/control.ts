// Control widget
// (c) Koheron

class Control {
    private frequency_value: HTMLLinkElement;
    private frequency_input: HTMLInputElement;
    private frequency_save: HTMLLinkElement;
    private frequency_slider: HTMLInputElement;

    private frequency: number;

    constructor(document: Document, private driver: FFT) {
        this.frequency_value = <HTMLLinkElement>document.getElementById('frequency-value');
        this.frequency_input = <HTMLInputElement>document.getElementById('frequency-input');
        this.frequency_save = <HTMLLinkElement>document.getElementById('frequency-save');
        this.frequency_slider = <HTMLInputElement>document.getElementById('frequency-slider');
        this.update();
    }

    update() {
        this.driver.getControlParameters( (sts: IFFTStatus) => {
            this.frequency_value.innerHTML = (sts.dds_freq[0]/1e6).toFixed(6);
            this.frequency_slider.value = (sts.dds_freq[0]/1e6).toFixed(4);
            requestAnimationFrame( () => { this.update(); } )
        });
    }

    editFrequency() {
        this.frequency_value.style.display = 'none';
        this.frequency_input.style.display = 'inline';
        this.frequency_save.style.display = 'inline';
        this.frequency_input.value = this.frequency_value.innerHTML;
    }

    saveFrequency() {
        this.frequency_value.style.display = 'inline';
        this.frequency_input.style.display = 'none';
        this.frequency_save.style.display = 'none';
        this.frequency = Math.min(parseFloat(this.frequency_input.value), 250);
        this.driver.setDDSFreq(0, 1e6 * this.frequency);
    }

    saveFrequencyKey(event: KeyboardEvent) {
        if (event.keyCode == 13) {
            this.saveFrequency();
        }
    }

    slideFrequency() {
        if (this.frequency_input.style.display == 'inline') {
            this.frequency_value.style.display = 'inline';
            this.frequency_input.style.display = 'none';
            this.frequency_save.style.display = 'none';
        }

        this.frequency = parseFloat(this.frequency_slider.value);
        this.driver.setDDSFreq(0, 1e6 * this.frequency);
    }

}
