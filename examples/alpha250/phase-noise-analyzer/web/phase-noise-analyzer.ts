// Interface for the Phase Noise Analyzer driver
// (c) Koheron

type TupleGetParameters = [number, number, number, number, number, number, number, number, number, number];
type TupleGetTrackingParameters = [boolean, number, number, number, number, number, number, number, number, number, number, boolean, boolean];

interface ITrackingParameters {
  enabled: boolean;
  bandwidth: number;
  effectiveBandwidth: number;
  maxStep: number;
  maxCorrection: number;
  nominal0: number;
  nominal1: number;
  correction0: number;
  correction1: number;
  error0: number;
  error1: number;
  locked0: boolean;
  locked1: boolean;
}

interface IParameters {
  data_size: number; // fft_size/2 + 1 (includes DC and Nyquist)
  fs: number;        // Sampling frequency (Hz)
  channel: number;   // Acquired channel
  cic_rate: number;
  fft_navg: number;
  fdds0: number;
  fdds1: number;
  analyzer_mode: string;
  interferometer_delay: number;
  clkIndex: string;
}

type TupleGetMeasurements = [number, number, number, number, number];

interface IMeasurements {
  phase_jitter: number; // rad rms
  time_jitter: number;  // s rms
  freq_lo: number; // Integration interval start
  freq_hi: number; // Integration interval end
  carrier_power: number;
}

interface IAverageStatus {
  count: number;
  target: number;
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
  }

  async getParameters(): Promise<IParameters> {
    const [data_size, fs, channel, cic_rate, fft_navg, fdds0, fdds1, mode, interferometer_delay, clkin] =
      await this.client.readTuple<TupleGetParameters>(
        Command(this.id, this.cmds['get_parameters']),
        'IfIIIddIfI'
      );

    const analyzer_mode = (mode == 0 ?  'rf' : 'laser');

    let clkIndex: string = "0";

    if (clkin !== 0) {
      clkIndex = "2";
    }

    this.parameters = { data_size, fs, channel, cic_rate, fft_navg, fdds0, fdds1, analyzer_mode, interferometer_delay, clkIndex };
    return this.parameters;
  }

  async getMeasurements(nAverage: number): Promise<IMeasurements> {
    const [phase_jitter, time_jitter, freq_lo, freq_hi, carrier_power] =
      await this.client.readTuple<TupleGetMeasurements>(
        Command(this.id, this.cmds['get_measurements'], nAverage),
        'ffffd'
      );

    return { phase_jitter, time_jitter, freq_lo, freq_hi, carrier_power };
  }

  setFFTNavg(navg: number): void {
    this.client.send(Command(this.id, this.cmds['set_fft_navg'], navg));
  }

  async getAverageStatus(): Promise<IAverageStatus> {
    const [count, target] = await this.client.readTuple<[number, number]>(
      Command(this.id, this.cmds['get_average_status']), 'II');
    return {count, target};
  }

  setLocalOscillator(channel: number, freqHz: number): void {
    this.client.send(Command(this.id, this.cmds['set_local_oscillator'], channel, freqHz));
  }

  setTrackingEnabled(enabled: boolean): void {
    this.client.send(Command(this.id, this.cmds['set_tracking_enabled'], enabled));
  }

  async getTrackingParameters(): Promise<ITrackingParameters> {
    const [enabled, bandwidth, effectiveBandwidth, maxStep, maxCorrection,
      nominal0, nominal1, correction0, correction1, error0, error1, locked0, locked1] =
      await this.client.readTuple<TupleGetTrackingParameters>(
        Command(this.id, this.cmds['get_tracking_parameters']), '?dddddddddd??');
    return {enabled, bandwidth, effectiveBandwidth, maxStep, maxCorrection,
      nominal0, nominal1, correction0, correction1, error0, error1, locked0, locked1};
  }

  async getPhaseNoise(): Promise<Float32Array> {
    return await this.client.readFloat32Vector(Command(this.id, this.cmds['get_phase_noise']));
  }

  setCicRate(cic_rate: number): void {
    this.client.send(Command(this.id, this.cmds['set_cic_rate'], cic_rate));
  }

  setChannel(channel: number): void {
    this.client.send(Command(this.id, this.cmds['set_channel'], channel));
  }

  setAnalyzerMode(mode: number): void {
    this.client.send(Command(this.id, this.cmds['set_analyzer_mode'], mode));
  }

  setInterferometerDelay(delay_s: number): void {
    this.client.send(Command(this.id, this.cmds['set_interferometer_delay'], delay_s));
  }

  async getCarrierPower(nAverage: number): Promise<number> {
    return await this.client.readFloat64(
      Command(this.id, this.cmds['get_carrier_power'], nAverage)
    );
  }

  saveConfig(): void {
    this.client.send(Command(this.id, this.cmds['save_config']));
  }
}
