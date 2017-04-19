// Modulation
// (c) Koheron

interface ModulationStatus {
    wfmType: number[];
    dacAmplitude: number[];
    dacFrequency: number[];
    dacOffset: number[];
}

class ModulationDriver {

    private driver: Driver;
    private id: number;
    private cmds: Commands;

    constructor(private client: Client) {
        this.driver = this.client.getDriver('Modulation');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getModulationStatus(cb: (status: ModulationStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_modulation_status']), 'IIffffff', (tup: number[]) => {
            let status: ModulationStatus = <ModulationStatus>{};
            status.wfmType = [];
            status.wfmType[0] = tup[0];
            status.wfmType[1] = tup[1];
            status.dacAmplitude = [];
            status.dacAmplitude[0] = tup[2];
            status.dacAmplitude[1] = tup[3];
            status.dacFrequency = [];
            status.dacFrequency[0] = tup[4];
            status.dacFrequency[1] = tup[5];
            status.dacOffset = [];
            status.dacOffset[0] = tup[6];
            status.dacOffset[1] = tup[7];
            cb(status);
        });
    }

    getWfmSize(cb: (wfm_size: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_wfm_size']), (wfm_size) => {
            cb(wfm_size)
        });
    }

    setWaveformType(channel: number, wfmType: number): void {
        this.client.send(Command(this.id, this.cmds['set_waveform_type'], channel, wfmType));
    }

    setDacAmplitude(channel: number, dacAmplitude: number): void {
        this.client.send(Command(this.id, this.cmds['set_dac_amplitude'], channel, dacAmplitude));
    }

    setDacFrequency(channel: number, dacFrequency: number): void {
        this.client.send(Command(this.id, this.cmds['set_dac_frequency'], channel, dacFrequency));
    }

    setDacOffset(channel: number, dacOffset: number): void {
        this.client.send(Command(this.id, this.cmds['set_dac_offset'], channel, dacOffset));
    }

}

class ModulationControl {

    private waveformButtons: HTMLLinkElement[][];
    private amplitudeSlider: HTMLInputElement[];
    private frequencySlider: HTMLInputElement[];
    private offsetSlider: HTMLInputElement[];
    private frequencySpan: HTMLSpanElement[];
    private amplitudeSpan: HTMLSpanElement[];
    private offsetSpan: HTMLSpanElement[];

    readonly waveformTypes = ['sine', 'triangle', 'square'];

    constructor(document: Document, private driver: ModulationDriver, private wfmSize: number, private samplingRate: number) {

        this.waveformButtons = [];
        this.amplitudeSlider = [];
        this.frequencySlider = [];
        this.offsetSlider = [];
        this.frequencySpan = [];
        this.amplitudeSpan = [];
        this.offsetSpan = [];

        for (let channel: number = 0; channel < 2; channel++) {
            this.waveformButtons[channel] = [];
            this.waveformTypes.forEach((waveformType, index) => {
                this.waveformButtons[channel][index] = <HTMLLinkElement>document.getElementById(waveformType + '-' + channel.toString());
            });
            this.amplitudeSlider[channel] =<HTMLInputElement> document.getElementById('amplitude-slider-' + channel.toString());
            this.frequencySlider[channel] = <HTMLInputElement>document.getElementById('frequency-slider-' + channel.toString());
            this.offsetSlider[channel] = <HTMLInputElement>document.getElementById('offset-slider-' + channel.toString());
            this.frequencySpan[channel] = <HTMLSpanElement>document.getElementById('frequency-' + channel.toString());
            this.amplitudeSpan[channel] = <HTMLSpanElement>document.getElementById('amplitude-' + channel.toString());
            this.offsetSpan[channel] = <HTMLSpanElement>document.getElementById('offset-' + channel.toString());
        }

        this.update();
    }

    update() {
        this.driver.getModulationStatus((status: ModulationStatus) => {
            for (let channel: number = 0; channel < 2; channel++) {

                for (let i = 0; i < this.waveformTypes.length; i++) {
                    if (status.wfmType[channel] === i) {
                        this.waveformButtons[channel][i].className = 'btn btn-primary-reversed active';
                    } else {
                        this.waveformButtons[channel][i].className = 'btn btn-primary-reversed';
                    }
                }

                const amplitude = status.dacAmplitude[channel].toFixed(3).toString();
                this.amplitudeSpan[channel].innerHTML = amplitude;
                this.amplitudeSlider[channel].value = amplitude;

                const frequency = status.dacFrequency[channel]
                this.frequencySpan[channel].innerHTML = (frequency / 1e6).toFixed(3).toString();
                this.frequencySlider[channel].value = (frequency * this.wfmSize / this.samplingRate).toString();

                const offset = status.dacOffset[channel].toFixed(3).toString();
                this.offsetSpan[channel].innerHTML = offset;
                this.offsetSlider[channel].value = offset;
            }
            requestAnimationFrame( () => { this.update(); } )
        });
    }

    // Waveform

    setWfmType(channel: number, wfmType: string): void {
        this.driver.setWaveformType(channel, this.waveformTypes.findIndex(_ => _ === wfmType));
    }

    setDacAmplitude(channel: number): void {
        let amplitude = parseFloat(this.amplitudeSlider[channel].value);
        this.driver.setDacAmplitude(channel, amplitude);
    }

    setDacFrequency(channel: number): void {
        let frequency = parseInt(this.frequencySlider[channel].value) * this.samplingRate / this.wfmSize;
        this.driver.setDacFrequency(channel, frequency);
    }

    setDacOffset(channel: number): void {
        let offset = parseFloat(this.offsetSlider[channel].value);
        this.driver.setDacOffset(channel, offset);
    }

}

