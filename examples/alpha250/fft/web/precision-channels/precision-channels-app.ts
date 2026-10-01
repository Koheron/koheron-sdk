class PrecisionChannelsApp {
    constructor(private document: Document, private precisionDac: PrecisionDac) {
        for (const input of Array.from(document.getElementsByClassName('precision-dac-input')) as HTMLInputElement[]) {
            input.addEventListener(input.type === 'range' ? 'input' : 'change', () => {
                const millivolts = input.valueAsNumber;
                if (!Number.isFinite(millivolts) || !input.checkValidity()) { return; }
                const command = input.dataset.command;
                const channel = input.dataset.channel;
                const counterpartType = input.type === 'number' ? 'range' : 'number';
                const counterpart = this.document.querySelector<HTMLInputElement>(
                    `[data-command='${command}'][data-channel='${channel}'][type='${counterpartType}']`
                );
                if (counterpart) { counterpart.value = input.value; }
                this.precisionDac[command](channel, millivolts / 1000);
            });
        }
    }
}
