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

    private channelNum: number = 2;
    private currentChannel: number;
    private channelInputs: HTMLInputElement[];
    private channelElements: any[];
    private modulationInputs: HTMLInputElement[];

    constructor(document: Document, private driver: ModulationDriver, private wfmSize: number, private samplingRate: number) {

        this.channelInputs = <HTMLInputElement[]><any>document.getElementsByClassName("modulation-channel-input");
        this.initChannelInputs();
        this.currentChannel = parseInt((<HTMLInputElement>document.querySelector(".modulation-channel-input:checked")).value);

        this.channelElements = <any[]><any>document.getElementsByClassName("modulation-channel");
        this.modulationInputs = <HTMLInputElement[]><any>document.getElementsByClassName("modulation-input");
        this.initModulationInputs();
        this.update();
    }

    update() {
        this.driver.getModulationStatus((status: ModulationStatus) => {

            for (let property in status) {
                if (status.hasOwnProperty(property)) {
                    if (["dacAmplitude", "dacFrequency", "dacOffset"].indexOf(property) > -1) {
                        let input = document.querySelector("input.modulation-channel[data-status='" + property + "'][data-channel='" + this.currentChannel.toString() + "']");
                        let span = document.querySelector("span.modulation-channel[data-status='" + property + "'][data-channel='" + this.currentChannel.toString() + "']");
                        let inputValue: string;
                        let spanValue: string;

                        if (property === "dacAmplitude") {
                            inputValue = status[property][this.currentChannel].toFixed(3);
                            spanValue = inputValue;
                        } else if (property === "dacFrequency") {
                            inputValue = (status[property][this.currentChannel] * this.wfmSize / this.samplingRate).toFixed(3);
                            spanValue = (status[property][this.currentChannel] * this.wfmSize / this.samplingRate).toFixed(3);
                        } else if (property === "dacOffset") {
                            inputValue = (status[property][this.currentChannel] / 1e6).toFixed(3);
                            spanValue = inputValue;
                        }

                        if (document.activeElement !== input && document.body.contains(input)) {
                            (<HTMLInputElement>input).value = inputValue;
                        }

                        if (document.body.contains(span)) {
                            (<HTMLSpanElement>span).textContent = spanValue;
                        }
                    } else if (property === "wfmType") {
                        let input = <HTMLInputElement>document.querySelector("input.modulation-channel[data-status='" + property + "'][data-channel='" + this.currentChannel.toString() + "'][value='" + status[property][this.currentChannel].toString() + "']");
                        input.checked = true;
                    }
                }
            }

            requestAnimationFrame( () => { this.update(); } )
        });
    }

    initChannelInputs(): void {
        for (let i = 0; i < this.channelInputs.length; i ++) {
            this.channelInputs[i].addEventListener('change', (event) => {
                for (let j = 0; j < this.channelElements.length; j++) {
                    let channel: string = (<HTMLInputElement>event.currentTarget).value;
                    (<any>this.channelElements[j]).dataset.channel = channel;
                    this.currentChannel = parseInt(channel);
                }
            })
        }
    }

    initModulationInputs(): void {
        let events = ['change', 'input'];
        for (let i = 0; i < events.length; i++) {
            for (let j = 0; j < this.modulationInputs.length ; j++) {
                this.modulationInputs[j].addEventListener(events[i], (event) => {
                    let command: string = (<HTMLInputElement>event.currentTarget).dataset.command;
                    let channel: string = (<HTMLInputElement>event.currentTarget).dataset.channel;
                    let value: string = (<HTMLInputElement>event.currentTarget).value;
                    let arg: any;
                    let status: string;

                    if (command === "setWaveformType") {
                        arg = parseInt(value);
                    } else if (command === "setDacAmplitude") {
                        arg = parseFloat(value);
                    } else if (command === "setDacFrequency") {
                        arg = parseInt(value) * this.samplingRate / this.wfmSize;
                    } else if (command === "setDacOffset") {
                        arg = parseFloat(value);
                    }

                    this.driver[command](channel, arg);
                })
            }
        }


    }

}

