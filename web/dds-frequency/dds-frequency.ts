interface DDSFrequencyPort {
    setDDSFreq(channel: number, frequency: number): void;
    getControlParameters(): Promise<{dds_freq: number[]}>;
}

// DDS frequency editing shares the instrument digit editor; commands stay in Hz.
class DDSFrequency {
    private editors: FrequencyInput[] = [];
    private poller: InstrumentPoller<{dds_freq: number[]}>;
    private disposed = false;

    constructor(private document: Document, private driver: DDSFrequencyPort) {
        for (const input of Array.from(document.querySelectorAll<HTMLInputElement>('.dds-channel-input'))) {
            const channel = Number(input.dataset.channel);
            this.editors[channel] = new FrequencyInput(input,
                document.querySelector<HTMLSelectElement>(`.dds-frequency-unit[data-channel='${channel}']`), {
                    value: 0, maximum: 125e6, inclusiveMaximum: true, resolution: 1,
                    commit: async frequency => {
                        this.driver.setDDSFreq(channel, frequency);
                        return (await this.driver.getControlParameters()).dds_freq[channel];
                    }
                });
        }
        this.poller = new InstrumentPoller(document, () => this.driver.getControlParameters(),
            values => this.setValues(values.dds_freq), error => {
                document.getElementById('dds-frequency-status').textContent = 'Readback unavailable; retrying…';
                console.error('DDS readback failed:', error);
            });
    }

    async init(): Promise<void> {
        const status = await this.driver.getControlParameters();
        if (this.disposed) { return; }
        this.setValues(status.dds_freq);
        (this.document.getElementById('dds-frequency-controls') as HTMLFieldSetElement).disabled = false;
        this.poller.start();
    }

    private setValues(values: number[]): void {
        this.editors.forEach((editor, channel) => editor.setValue(values[channel]));
        this.document.getElementById('dds-frequency-status').textContent = '';
    }

    dispose(): void {
        this.disposed = true;
        this.poller.dispose();
        this.editors.forEach(editor => editor.dispose());
        (this.document.getElementById('dds-frequency-controls') as HTMLFieldSetElement).disabled = true;
    }
}
