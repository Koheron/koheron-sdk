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

    async getControlParameters(): Promise<DualDDSStatus> {
        const tuple = await this.client.readTuple<[number, number]>(
            Command(this.id, this.cmds['get_control_parameters']), 'dd');
        return {dds_freq: [tuple[0], tuple[1]]};
    }
}
