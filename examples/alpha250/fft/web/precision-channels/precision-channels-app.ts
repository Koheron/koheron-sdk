class PrecisionChannelsApp {
    private numbers: {[channel: number]: NumberInput} = {};
    private disposed = false;

    constructor(private document: Document, private precisionDac: PrecisionDac) {
        for (const input of Array.from(document.getElementsByClassName('precision-dac-input')) as HTMLInputElement[]) {
            const channel = Number(input.dataset.channel);
            this.numbers[channel] = new NumberInput(input, {
                value: 0, minimum: 0, maximum: 2500, resolution: .001, unitLabel: 'mV',
                commit: async millivolts => {
                    this.precisionDac.setDac(channel, millivolts / 1000);
                    const values = await this.precisionDac.getDacValues();
                    return values[channel] * 1000;
                }
            });
        }
    }

    async init(): Promise<void> {
        this.setValues(await this.precisionDac.getDacValues());
    }

    setValues(values: ArrayLike<number>): void {
        if (this.disposed) { return; }
        for (const channel of Object.keys(this.numbers)) {
            this.numbers[channel].setValue(values[Number(channel)] * 1000);
        }
    }

    dispose(): void {
        this.disposed = true;
        Object.values(this.numbers).forEach(number => number.dispose());
    }
}
