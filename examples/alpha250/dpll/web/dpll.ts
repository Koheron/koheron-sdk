interface IDpllStatus {
  dds_freq: number[];
  p_gain: number[];
  pi_gain: number[];
  i2_gain: number[];
  i3_gain: number[];
  integrators: number[];
}


class Dpll {

  private driver: Driver;
  private id: number;
  private cmds: Commands;

  public status: IDpllStatus;

  constructor (private client: Client) {
    this.driver = this.client.getDriver('Dpll');
    this.id = this.driver.id;
    this.cmds = this.driver.getCmds();

    this.status = <IDpllStatus>{};
    this.status.dds_freq = [];
    this.status.p_gain = [];
    this.status.pi_gain = [];
    this.status.i2_gain = [];
    this.status.i3_gain = [];
    this.status.integrators = [];
  }

  setDDSFreq(channel: number, freq_hz: number): void {
    this.client.send(Command(this.id, this.cmds['set_dds_freq'], channel, freq_hz));
  }

  setIntegrator(channel: number, integrator_index: number, integrator_on: boolean): void {
    this.client.send(Command(this.id, this.cmds['set_integrator'], channel, integrator_index, integrator_on));
  }

  setPGain(channel: number, gain: number): void {
    this.client.send(Command(this.id, this.cmds['set_p_gain'], channel, gain));
  }

  setPiGain(channel: number, gain: number): void {
    this.client.send(Command(this.id, this.cmds['set_pi_gain'], channel, gain));
  }

  setI2Gain(channel: number, gain: number): void {
    this.client.send(Command(this.id, this.cmds['set_i2_gain'], channel, gain));
  }

  setI3Gain(channel: number, gain: number): void {
    this.client.send(Command(this.id, this.cmds['set_i3_gain'], channel, gain));
  }

  setDacOutput(channel: number, sel: number): void {
    this.client.send(Command(this.id, this.cmds['set_dac_output'], channel, sel));
  }

  getControlParameters(cb: (status: IDpllStatus) => void): void {
    this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'ddiiiiiiiiII',
                           (tup: any) => {
        this.status.dds_freq[0] = tup[0];
        this.status.dds_freq[1] = tup[1];
        this.status.p_gain[0] = tup[2];
        this.status.p_gain[1] = tup[3];
        this.status.pi_gain[0] = tup[4];
        this.status.pi_gain[1] = tup[5];
        this.status.i2_gain[0] = tup[6];
        this.status.i2_gain[1] = tup[7];
        this.status.i3_gain[0] = tup[8];
        this.status.i3_gain[1] = tup[9];
        this.status.integrators[0] = tup[10];
        this.status.integrators[1] = tup[11];
        cb(this.status);
    });
  }

}