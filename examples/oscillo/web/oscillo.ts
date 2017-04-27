// Interface for the Oscillo driver
// (c) Koheron

class Oscillo {
    readonly samplingRate: number = 125e6;
    readonly mhz: number = 1e6;
    readonly wfmSize: number = 8192;
    readonly maxT: number;

    private decimationFactor: number;
    private indexLow: number;
    private indexHigh: number;

    private driver: Driver;
    private id: number;
    private cmds: Commands;

    constructor(private client: Client) {
        this.maxT = this.mhz * (this.wfmSize - 1) / this.samplingRate;

        this.decimationFactor = 1;
        this.indexLow = 0;
        this.indexHigh = this.wfmSize - 1;

        this.driver = this.client.getDriver('Oscillo');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    setTimeRange(time_range: jquery.flot.range): void {
        const indexLow_  = this.timeToIndex(time_range.from);
        const indexHigh_ = this.timeToIndex(time_range.to);

        if (indexHigh_ <= indexLow_ || indexHigh_ >= this.wfmSize) {
            console.error('Invalid range low = ' + time_range.from.toString()
                          + ' high = ' + time_range.to.toString());
            return;
        }

        if (indexHigh_ <= indexLow_ + 1) {
            return;
        }

        this.indexLow  = indexLow_;
        this.indexHigh = indexHigh_;

        // Decimation factor selection
        this.decimationFactor = Math.ceil((this.indexHigh - this.indexLow) / 8192);
    }

    timeToIndex(t: number): number {
        return Math.floor(t * this.samplingRate / this.mhz);
    }

    getData(callback: (ch0: number[][], ch1: number[][], range: jquery.flot.range) => void): void {
        this.client.readFloat32Vector(
            Command(this.id, this.cmds['get_data_decim'],
            this.decimationFactor,
            this.indexLow,
            this.indexHigh), (array) => {
                const t0: number = this.indexLow * this.mhz / this.samplingRate;
                const coeff: number = this.mhz * this.decimationFactor / this.samplingRate;
                const size: number = Math.floor((this.indexHigh - this.indexLow) / this.decimationFactor);

                let channel0: number[][] = [];
                let channel1: number[][] = [];
                let range = <jquery.flot.range>{};

                if (array.length / 2 === size) {
                    channel0 = new Array(size);
                    channel1 = new Array(size);

                    for (let i: number = 0; i < size; i++) {
                        channel0[i] = [t0 + coeff * i, array[i]];
                        channel1[i] = [t0 + coeff * i, array[i + size]];
                    }

                    // Exact range used in µs
                    range = {
                        from: t0,
                        to: this.indexHigh * this.mhz / this.samplingRate
                    };
                }
                callback(channel0, channel1, range);
            }
        );
    }

    // Averaging

    getAverageStatus(cb: (status: AverageStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_average_status']), '?II',
                              (tup: [boolean, number, number]) => {
            let status: AverageStatus = <AverageStatus>{};
            status.isAverage = tup[0];
            status.numAverageMin = tup[1];
            status.numAverage = tup[2];
            cb(status);
        });
    }

    setAverage(status: boolean): void {
        this.client.send(Command(this.id, this.cmds['set_average'], status));
    }

    setNumAverageMin(num_average_min: number): void {
        this.client.send(Command(this.id, this.cmds['set_num_average_min'], num_average_min));
    }

}