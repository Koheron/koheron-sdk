interface IDpllStatus {
  dds_freq: number[];
  p_gain: number[];
  pi_gain: number[];
  i2_gain: number[];
  i3_gain: number[];
  integrators: number[];
  p_path: number[];
}


class PModeError extends Error {
  constructor(public code: number, message: string) { super(message); }
}

class Dpll {

  private driver: Driver;
  private id: number;
  private cmds: Commands;

  constructor (private client: Client) {
    this.driver = this.client.getDriver('Dpll');
    this.id = this.driver.id;
    this.cmds = this.driver.getCmds();
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

  async getControlParameters(): Promise<IDpllStatus> {
    const [tup, gains, paths] = await Promise.all([
      this.client.readTuple(Command(this.id, this.cmds['get_control_parameters']), 'ddiiiiiiiiII'),
      this.client.readFloat64Array(Command(this.id, this.cmds['get_gain_values'])),
      this.client.readUint32Array(Command(this.id, this.cmds['get_p_path_status']))
    ]);
    if (gains.length !== 8 || !Array.from(gains).every(Number.isFinite)) {
      throw new Error('Invalid gain readback.');
    }
    if (paths.length !== 2) { throw new Error('Invalid P path readback.'); }
    return {
      dds_freq: [tup[0], tup[1]],
      p_gain: [gains[0], gains[1]],
      pi_gain: [gains[2], gains[3]],
      i2_gain: [gains[4], gains[5]],
      i3_gain: [gains[6], gains[7]],
      integrators: [tup[10], tup[11]],
      p_path: Array.from(paths)
    };
  }

  getDacOutputs(): Promise<number[]> {
    return this.client.readTuple(Command(this.id, this.cmds['get_dac_outputs']), 'II');
  }

  async setGeometricGain(channel: number, gain: number, sign: number, step: number): Promise<void> {
    const result = await this.client.readInt32(Command(this.id, this.cmds['set_geometric_gain'], channel, gain, sign, step));
    if (result !== 0) { throw new Error(`Gain update failed (${result}).`); }
  }

  async setPMode(channel: number, mode: number): Promise<void> {
    const result = await this.client.readInt32(Command(this.id, this.cmds['set_p_mode'], channel, mode));
    if (result !== 0) {
      throw new PModeError(result, result === -3 ? 'Enable integrators 0 and 2 and provide a stable signal before selecting Fast P + I.' :
        `P path update failed (${result}).`);
    }
  }
}
