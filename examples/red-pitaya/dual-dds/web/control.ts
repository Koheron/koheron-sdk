// Control widget
// (c) Koheron

interface DualDDSStatus {
    dds_freq: number[];
}

class Control {
    constructor(document: Document, private driver) {
        this.update();
    }

    update() {
        this.driver.getControlParameters( (status: DualDDSStatus) => {
            for (let i: number = 0; i < status.dds_freq.length; i++) {
                let inputs = document.querySelectorAll(".dds-channel-input[data-channel='" + i.toString() + "']");
                for (let j: number = 0; j < inputs.length; j++) {
                    if (document.activeElement !== inputs[j]) {
                        (<HTMLInputElement>inputs[j]).value =  (status.dds_freq[i] / 1e6).toFixed(6);
                    }
                }
            }
            requestAnimationFrame( () => { this.update(); } )
        });
    }
}