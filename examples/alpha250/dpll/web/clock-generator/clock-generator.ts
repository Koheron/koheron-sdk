class ClockGenerator {
  private driver: Driver;
  private id: number;
  private cmds: Commands;

  constructor(private client: Client) {
    this.driver = client.getDriver('ClockGenerator');
    this.id = this.driver.id;
    this.cmds = this.driver.getCmds();
  }

  getReferenceClock(): Promise<number> {
    return this.client.readUint32(Command(this.id, this.cmds['get_reference_clock']));
  }

  getDacSamplingFrequency(): Promise<number> {
    return this.client.readFloat64(Command(this.id, this.cmds['get_dac_sampling_freq']));
  }

  setReferenceClock(clkin: number): void {
    this.client.send(Command(this.id, this.cmds['set_reference_clock'], clkin));
  }
}
