// Interface for Ltc2387 driver
// (c) Koheron

class Ltc2387 {
    private driver: Driver;
    private id: number;
    private cmds: Commands;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('Ltc2387');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    rangeSelect(channel: number, range: number): void {
        this.client.send(Command(this.id, this.cmds['range_select'], channel, range));
    }

    async inputRange(channel: number): Promise<number> {
        return await this.client.readUint32(Command(this.id, this.cmds['input_range'], channel));
    }

    setInputRange(value: number): void {
        this.rangeSelect((value & 2) >> 1, value & 1);
    }
}