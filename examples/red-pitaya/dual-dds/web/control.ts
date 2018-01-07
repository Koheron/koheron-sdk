// Control widget
// (c) Koheron

class Control {
    private frequencyInputs: HTMLInputElement[];
    private frequencySliders: HTMLInputElement[];

    constructor(document: Document, private driver: DualDDS) {
        this.frequencyInputs = [];
        this.frequencySliders = [];
        for (let channel: number = 0; channel < 2; channel++) {
            this.frequencyInputs[channel] = <HTMLInputElement>document.getElementById('frequency-input-' + channel.toString());
            this.frequencySliders[channel] = <HTMLInputElement>document.getElementById('frequency-slider-'+ channel.toString());
        }
        this.update();
    }

    update() {
        this.driver.getControlParameters( (sts: DualDDSStatus) => {
            for (let channel: number = 0; channel < 2; channel++) {
                if (document.activeElement !== this.frequencyInputs[channel]) {
                    this.frequencyInputs[channel].value = (sts.dds_freq[channel] / 1e6).toFixed(6);
                }
                if (document.activeElement !== this.frequencySliders[channel]) {
                    this.frequencySliders[channel].value = (sts.dds_freq[channel]/1e6).toFixed(8);
                }
            }
            requestAnimationFrame( () => { this.update(); } )
        });
    }


    setFrequency(channel: number, event) {

        let frequencyValue = event.value;

        if (event.type === 'number') {
            this.frequencySliders[channel].value = frequencyValue;
        } else if (event.type === 'range') {
            this.frequencyInputs[channel].value = frequencyValue;
        }

        this.driver.setDDSFreq(channel, 1e6 * Math.min(parseFloat(frequencyValue), 62.5));
    }

}