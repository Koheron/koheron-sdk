// Control widget
// (c) Koheron

class Control {
    private frequency_value: HTMLLinkElement[];
    private frequency_input: HTMLInputElement[];
    private frequency_save: HTMLLinkElement[];
    private frequency_slider: HTMLInputElement[];

    constructor(document: Document, private driver: DualDDS) {
        this.frequency_value = [];
        this.frequency_input = [];
        this.frequency_save = [];
        this.frequency_slider = [];
        for (let channel: number = 0; channel < 2; channel++) {
            this.frequency_value[channel] = <HTMLLinkElement>document.getElementById('frequency-value-' + channel.toString());
            this.frequency_input[channel] = <HTMLInputElement>document.getElementById('frequency-input-' + channel.toString());
            this.frequency_save[channel] = <HTMLLinkElement>document.getElementById('frequency-save-' + channel.toString());
            this.frequency_slider[channel] = <HTMLInputElement>document.getElementById('frequency-slider-'+ channel.toString());
        }
        this.update();
    }

    update() {
        this.driver.getControlParameters( (sts: DualDDSStatus) => {
            for (let channel: number = 0; channel < 2; channel++) {
                this.frequency_value[channel].innerHTML = (sts.dds_freq[channel]/1e6).toFixed(8);
                this.frequency_slider[channel].value = (sts.dds_freq[channel]/1e6).toFixed(8);
            }
            requestAnimationFrame( () => { this.update(); } )
        });
    }

    editFrequency(channel: number) {
        this.frequency_value[channel].style.display = 'none';
        this.frequency_input[channel].style.display = 'inline';
        this.frequency_save[channel].style.display = 'inline';
        this.frequency_input[channel].value = this.frequency_value[channel].innerHTML;
    }

    saveFrequency(channel: number) {
        this.frequency_value[channel].style.display = 'inline';
        this.frequency_input[channel].style.display = 'none';
        this.frequency_save[channel].style.display = 'none';
        let frequency = Math.min(parseFloat(this.frequency_input[channel].value), 62.5);
        this.driver.setDDSFreq(channel, 1e6 * frequency);
    }

    saveFrequencyKey(event: KeyboardEvent, channel: number) {
        if (event.keyCode == 13) {
            this.saveFrequency(channel);
        }
    }

    slideFrequency(channel: number) {
        if (this.frequency_input[channel].style.display == 'inline') {
            this.frequency_value[channel].style.display = 'inline';
            this.frequency_input[channel].style.display = 'none';
            this.frequency_save[channel].style.display = 'none';
        }
        let frequency = parseFloat(this.frequency_slider[channel].value);
        this.driver.setDDSFreq(channel, 1e6 * frequency);
    }

}