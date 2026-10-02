// DDS frequency widget
// (c) Koheron

class DDSFrequency {
    private ddsChannelInputs: HTMLInputElement[];

    constructor(private document: Document, private driver) {
        this.ddsChannelInputs = Array.from(document.getElementsByClassName("dds-channel-input")) as HTMLInputElement[];
        this.initDDSChannelInputs();
    }

    initDDSChannelInputs(): void {
        for (const input of this.ddsChannelInputs) {
            // Commit typed numbers when editing finishes; sliders stay live.
            input.addEventListener(input.type === 'range' ? 'input' : 'change', () => {
                const frequency = input.valueAsNumber;
                if (!Number.isFinite(frequency) || !input.checkValidity()) { return; }
                const command = input.dataset.command;
                const channel = input.dataset.channel;
                const counterpartType = input.type === 'number' ? 'range' : 'number';
                const counterpart = this.document.querySelector<HTMLInputElement>(
                    `[data-command='${command}'][data-channel='${channel}'][type='${counterpartType}']`
                );
                if (counterpart) { counterpart.value = input.value; }
                this.driver[command](channel, 1e6 * frequency);
            });
        }
    }
}
