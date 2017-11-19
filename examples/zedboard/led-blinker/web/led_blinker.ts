class LedBlinker {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('LedBlinker');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getLeds(cb: (value: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_leds']),
                                 (value) => {cb(value)});
    }

    setLed(index:number, status: boolean): void {
        this.client.send(Command(this.id, this.cmds['set_led'], index, status));
    }

}