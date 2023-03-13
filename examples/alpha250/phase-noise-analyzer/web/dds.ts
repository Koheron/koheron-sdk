// Interface for DDS driver
// (c) Koheron

class DDS {
  private driver: Driver;
  private id: number;
  private cmds: Commands;

  constructor (private client: Client) {
    this.driver = this.client.getDriver('Dds');
    this.id = this.driver.id;
    this.cmds = this.driver.getCmds();
  }

  setDDSFreq(channel: number, freq_hz: number): void {
    this.client.send(Command(this.id, this.cmds['set_dds_freq'], channel, freq_hz));
  }

  getDDSFreq(channel: number, callback: (data: number) => void): void {
    this.client.readFloat32(Command(this.id, this.cmds['get_dds_freq'], channel),
                            (data: number) => { callback(data); });
  }
}
