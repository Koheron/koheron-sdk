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

    private channelDivs: HTMLDivElement[];
    private waveformInputs: HTMLInputElement[][];
    private amplitudeSlider: HTMLInputElement[];
    private frequencySlider: HTMLInputElement[];
    private offsetSlider: HTMLInputElement[];
    private frequencySpan: HTMLSpanElement[];
    private amplitudeSpan: HTMLSpanElement[];
    private offsetSpan: HTMLSpanElement[];

    // readonly waveformTypes = ['sine', 'triangle', 'square'];

    constructor(document: Document, private driver: ModulationDriver, private wfmSize: number, private samplingRate: number) {

        this.channelDivs = [];
        this.waveformInputs = [];
        this.amplitudeSlider = [];
        this.frequencySlider = [];
        this.offsetSlider = [];
        this.frequencySpan = [];
        this.amplitudeSpan = [];
        this.offsetSpan = [];

        for (let i: number = 0; i < 2; i++) {
            this.channelDivs[i] = <HTMLDivElement>document.getElementById("channel-" + i.toString());
            this.waveformInputs[i] = [];
            for (let j: number = 0; j < 3; j ++) {
                this.waveformInputs[i][j] = <HTMLInputElement>document.getElementById("waveform-" + i.toString() + "-" + j.toString());
            }
            this.amplitudeSlider[i] =<HTMLInputElement> document.getElementById('amplitude-slider-' + i.toString());
            this.frequencySlider[i] = <HTMLInputElement>document.getElementById('frequency-slider-' + i.toString());
            this.offsetSlider[i] = <HTMLInputElement>document.getElementById('offset-slider-' + i.toString());
            this.frequencySpan[i] = <HTMLSpanElement>document.getElementById('frequency-' + i.toString());
            this.amplitudeSpan[i] = <HTMLSpanElement>document.getElementById('amplitude-' + i.toString());
            this.offsetSpan[i] = <HTMLSpanElement>document.getElementById('offset-' + i.toString());
        }

        this.update();
    }

    update() {
        this.driver.getModulationStatus((status: ModulationStatus) => {
            for (let channel: number = 0; channel < 2; channel++) {

                for (let i = 0; i < 3; i++) {
                    if (status.wfmType[channel] === i) {
                        this.waveformInputs[channel][i].checked = true;
                    } else {
                        this.waveformInputs[channel][i].checked = false;
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

    switchChannel(channel: string) {
        for (let i: number = 0; i < 2; i++) {
            if (i === parseInt(channel)) {
                this.channelDivs[i].style.display = "block";
            } else {
                this.channelDivs[i].style.display = "none";
            }
        }
    }

    // Waveform

    setWfmType(channel: number, wfmIndex: string): void {
        this.driver.setWaveformType(channel, parseInt(wfmIndex));
    }

    setDacAmplitude(channel: number, amplitude: string): void {
        this.driver.setDacAmplitude(channel, parseFloat(amplitude));
    }

    setDacFrequency(channel: number, frequency: string): void {
        this.driver.setDacFrequency(channel, parseInt(frequency) * this.samplingRate / this.wfmSize);
    }

    setDacOffset(channel: number, offset: string): void {
        this.driver.setDacOffset(channel, parseFloat(offset));
    }

}

