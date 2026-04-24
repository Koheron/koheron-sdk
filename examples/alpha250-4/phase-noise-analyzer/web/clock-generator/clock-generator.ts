class ClockGenerator {
    private driver: Driver;
    private id: number;
    private cmds: Commands;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('ClockGenerator');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getReferenceClock(cb: (clkin: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_reference_clock']),
                                 (clkin: number) => {cb(clkin)});
    }

    setReferenceClock(clkin: number): void {
        this.client.send(Command(this.id, this.cmds['set_reference_clock'], clkin));
    }

    setSamplingFrequency(fs_select: number): void {
        this.client.send(Command(this.id, this.cmds['set_sampling_frequency'], fs_select));
    }
}