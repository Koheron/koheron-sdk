// Control widget
// (c) Koheron

class Control {
    private channelNum: number;

    private InChannelInputs: HTMLInputElement[];

    public fftWindowIndex: number;
    private fftWindowSelect: HTMLSelectElement;

    private frequencies: Array<number>;
    private frequencyInputs: HTMLInputElement[];
    private frequencySliders: HTMLInputElement[];

    constructor(document: Document, private fft: FFT) {
        this.channelNum = 2;

        this.InChannelInputs = [];
        this.frequencyInputs = [];
        this.frequencySliders = [];

        for (let i: number = 0; i < this.channelNum; i++) {
            this.InChannelInputs[i] = <HTMLInputElement>document.getElementById('in-channel-' + i.toString());
            this.frequencyInputs[i] = <HTMLInputElement>document.getElementById('frequency-input-' + i.toString());
            this.frequencySliders[i] = <HTMLInputElement>document.getElementById('frequency-slider-' + i.toString());
        }

        this.frequencies = new Array(this.channelNum);

        this.fftWindowIndex = 1;
        this.fftWindowSelect = <HTMLSelectElement>document.getElementById("fft-window");
        this.init();
    }

    init() {
        this.fft.getControlParameters( (sts: IFFTStatus) => {
            for (let i: number = 0; i < this.channelNum; i++) {
                this.frequencyInputs[i].value = (sts.dds_freq[0] / 1e6).toFixed(6);
                this.frequencySliders[i].value = (sts.dds_freq[0] / 1e6).toFixed(4);
            }

            this.fft.getFFTWindowIndex( (windowIndex: number) => {
                this.fftWindowIndex = windowIndex;
                this.fftWindowSelect.value = windowIndex.toString();

                requestAnimationFrame( () => {this.update();} )
            });
        });
    }

    private update() {
        this.fft.getControlParameters( (sts: IFFTStatus) => {
            for (let i: number = 0; i < this.channelNum; i++) {
                if (document.activeElement !== this.frequencyInputs[i]) {
                    this.frequencyInputs[i].value = (sts.dds_freq[i] / 1e6).toFixed(6);
                }

                if (document.activeElement !== this.frequencySliders[i]) {
                    this.frequencySliders[i].value = (sts.dds_freq[i] / 1e6).toFixed(6);
                    this.frequencySliders[i].max = (sts.fs / 1e6 / 2).toFixed(1);
                }

                this.InChannelInputs[sts.channel].checked = true;

            }

            this.fft.getFFTWindowIndex( (windowIndex: number) => {
                this.fftWindowSelect.value = windowIndex.toString();
                requestAnimationFrame( () => { this.update(); } )
            });
        });
    }

    updateFrequency(channel: number, event) {
        let frequencyValue = event.value;

        if (event.type === 'number') {
            this.frequencySliders[channel].value = frequencyValue;
        } else if (event.type === 'range') {
            this.frequencyInputs[channel].value = frequencyValue;
        }

        this.fft.setDDSFreq(channel, 1e6 * parseFloat(frequencyValue));
    }

    setInChannel(channel: number) {
        this.fft.setInChannel(channel);
    }

    setFFTWindow(windowIndex: number) {
        this.fft.setFFTWindow(windowIndex);
    }

}