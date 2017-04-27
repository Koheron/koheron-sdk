// Interface for the Spectrum driver
// (c) Koheron

class Spectrum {

    private driver: Driver;
    private id: number;
    private cmds: Commands;

    readonly wfmSize: number;
    readonly samplingRate: number;
    readonly mhz: number;

    private rangeFreq: jquery.flot.range;
    private indexLow: number;
    private indexHigh: number;

    constructor(private client: Client) {

        this.driver = this.client.getDriver('Spectrum');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();

        this.wfmSize = 4096;
        this.samplingRate = 125e6;
        this.mhz = 1e6;

        this.rangeFreq = {
            from: 0,
            to: this.samplingRate / this.mhz / 2
        }

        this.setFreqRange(this.rangeFreq);
    }

    getData(callback: (data: number[][], range: jquery.flot.range) => void): void {
        this.client.readFloat32Vector(
            Command(this.id, this.cmds['get_data_decim'], 1, this.indexLow, this.indexHigh), (array) => {

                const size: number = this.indexHigh - this.indexLow;
                let data: number[][] = [];
                let freq: number;
                let psd: number;
                let range: jquery.flot.range;

                if (array.length === size) {
                    data = new Array(size);

                    for (let i: number = 0; i < size; i++) {
                        freq = (this.indexLow + i) * this.samplingRate / this.wfmSize / this.mhz;
                        psd = 10 * Math.log10(array[i]);
                        data[i] = [freq, psd];
                    }

                    // Exact range used in µs
                    range = {
                        from: this.indexLow * this.samplingRate / this.wfmSize / this.mhz,
                        to: this.indexHigh * this.samplingRate / this.wfmSize / this.mhz
                    };
                }
                callback(data, range);
        });
    }

    getFreqRange(): jquery.flot.range {
        return this.rangeFreq;
    }

    setFreqRange(rangeFreq_: jquery.flot.range): void {
        this.rangeFreq = rangeFreq_;
        this.indexLow = Math.max(this.freqToIdx(rangeFreq_.from * this.mhz), 1);
        this.indexHigh = this.freqToIdx(rangeFreq_.to * this.mhz);;
    }

    freqToIdx(f0: number): number {
        return Math.floor(f0 / this.samplingRate * this.wfmSize);
    }

    // Averaging

    getAverageStatus(cb: (status: AverageStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_average_status']), '?II',
                              (tup: [boolean, number, number]) => {
            let status: AverageStatus = <AverageStatus>{};
            status.avgOn = tup[0];
            status.nMinAvg = tup[1];
            status.nAvg = tup[2];
            cb(status);
        });
    }

    setAvg(status: boolean): void {
        this.client.send(Command(this.id, this.cmds['set_average'], status));
    }

    setNAvgMin(n_min: number): void {
        this.client.send(Command(this.id, this.cmds['set_num_average_min'], n_min));
    }

}