// Interface for the Phase Noise Analyzer driver
// (c) Koheron

interface IParameters {
  data_size: number; // fft_size/2
  fs: number;        // Sampling frequency (Hz)
  channel: number;   // Acquired channel
  cic_rate: number;
  fft_navg: number;
}

class PhaseNoiseAnalyzer {
  private driver: Driver;
  private id: number;
  private cmds: Commands;

  public parameters: IParameters;

  constructor(private client: Client) {
    this.driver = this.client.getDriver('PhaseNoiseAnalyzer');
    this.id = this.driver.id;
    this.cmds = this.driver.getCmds();
    this.parameters = <IParameters>{};
  }

  getData(callback: (data: Uint32Array) => void): void {
    this.client.readUint32Array(Command(this.id, this.cmds['get_data']), (data: Uint32Array) => {
      callback(data);
    });
  }

  start(callback: () => void): void {
    this.client.send(Command(this.id, this.cmds['start']));
    callback();
  }

  getParameters(callback: (parameters: IParameters) => void): void {
    this.client.readTuple(Command(this.id, this.cmds['get_parameters']), 'IfIII',
    (tup: [number, number, number, number, number]) => {
      this.parameters.data_size = tup[0];
      this.parameters.fs = tup[1];
      this.parameters.channel = tup[2];
      this.parameters.cic_rate = tup[3];
      this.parameters.fft_navg = tup[4];
      callback(this.parameters);
    });
  }

  setFFTNavg(navg: number): void {
    this.client.send(Command(this.id, this.cmds['set_fft_navg'], navg));
  }

  getPhaseNoise(callback: (data: Float32Array) => void): void {
    this.client.readFloat32Array(Command(this.id, this.cmds['get_phase_noise']), (data: Float32Array) => {
      callback(data);
    });
  }

  setCicRate(cic_rate: number): void {
    this.client.send(Command(this.id, this.cmds['set_cic_rate'], cic_rate));
  }

  setChannel(channel: number): void {
    this.client.send(Command(this.id, this.cmds['set_channel'], channel));
  }

  getCarrierPower(nAverage: number, callback: (data: number) => void): void {
    this.client.readFloat32(Command(this.id, this.cmds['get_carrier_power'], nAverage), (data: number) => {
      callback(data);
    });
  }
}
