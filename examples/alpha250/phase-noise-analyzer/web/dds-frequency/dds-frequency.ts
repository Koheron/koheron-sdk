// DDS Frequency wiget
// (c) Koheron

class DDSFrequency {
    private ddsChannelInputs: HTMLInputElement[];

    constructor(document, private driver) {
        this.ddsChannelInputs = <HTMLInputElement[]><any>document.getElementsByClassName("dds-channel-input");
        this.initDDSChannelInputs();
    }

    initDDSChannelInputs(): void {
        let events = ['change', 'input'];
        for (let j = 0; j < events.length; j++) {
            for (let i = 0; i < this.ddsChannelInputs.length; i++) {
                this.ddsChannelInputs[i].addEventListener(events[j], (event) => {
                    let counterType: string = "number";

                    if ((<HTMLInputElement>event.currentTarget).type == "number") {
                        counterType = "range";
                    }

                    let command = (<HTMLInputElement>event.currentTarget).dataset.command;
                    let channel = (<HTMLInputElement>event.currentTarget).dataset.channel;
                    let value = (<HTMLInputElement>event.currentTarget).value;
                    (<HTMLInputElement>document.querySelector("[data-command='" + command + "'][data-channel='" + channel +"'][type='" + counterType + "']")).value = value ;
                    this.driver[command](channel, 1e6 * parseFloat(value));
                })
            }
        }
    }

}