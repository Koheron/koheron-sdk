// Interface for the Decimator driver
// (c) Koheron

class Decimator {
    private driver: Driver;
    private id: number;
    private cmds: Commands;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('Decimator');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    read_adc(cb: (data: Int32Array) => void): void {
        this.client.readInt32Array(Command(this.id, this.cmds['read_adc']),
            (data: Int32Array) => {
                cb(data);
            });
    }
}
