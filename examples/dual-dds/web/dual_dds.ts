// Interface for the DDSFFT driver
// (c) Koheron

interface DualDDSStatus {
    dds_freq: number[];
}

class DualDDS {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('DualDDS');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    setDDSFreq(channel: number, freq_hz: number): void {
        this.client.send(Command(this.id, this.cmds['set_dds_freq'], channel, freq_hz));
    }

    getControlParameters(cb: (status: DualDDSStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'dd',
                               (tup: [number, number]) => {
            let status: DualDDSStatus = <DualDDSStatus>{};
            status.dds_freq = [];
            status.dds_freq[0] = tup[0];
            status.dds_freq[1] = tup[1];
            cb(status);
        });
    }

}