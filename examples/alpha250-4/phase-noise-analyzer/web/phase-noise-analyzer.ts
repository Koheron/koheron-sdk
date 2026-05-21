// Interface for the Phase Noise Analyzer driver
// (c) Koheron

type TupleGetParameters = [number, number, number, number, number, number, number, number, number, number, number];

interface IParameters {
  data_size: number; // fft_size/2
  fs: number;        // Sampling frequency (Hz)
  channel: number;   // Acquired channel
  min_freq: number;
  fft_navg: number;
  fdds0: number;
  fdds1: number;
  fdds2: number;
  fdds3: number;
  clkIndex: string;
  avgxy_count: number;
}

type TupleGetMeasurements = [number, number, number, number, number];
type TupleGetTrackingParameters = [boolean, number, number, number, number, number, number, boolean];

interface IMeasurements {
  phase_jitter: number; // rad rms
  time_jitter: number;  // s rms
  freq_lo: number; // Integration interval start
  freq_hi: number; // Integration interval end
  carrier_power: number;
}

interface ITrackingParameters {
  tracking_enabled: boolean;
  tracking_bandwidth: number;
  effective_tracking_bandwidth: number;
  tracking_correction_x: number;
  tracking_correction_y: number;
  tracking_last_mean_dphi: number;
  tracking_last_error: number;
  tracking_locked: boolean;
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
    const [data_size, fs, channel, min_freq, fft_navg, fdds0, fdds1, fdds2, fdds3, clkin, avgxy_count] =
      await this.client.readTuple<TupleGetParameters>(
        Command(this.id, this.cmds['get_parameters']),
        'IdIdIddddII'
      );

    let clkIndex: string = "0";

    if (clkin !== 0) {
      clkIndex = "2";
    }

    this.parameters = { data_size, fs, channel, min_freq, fft_navg, fdds0, fdds1, fdds2, fdds3, clkIndex, avgxy_count };
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

  setLocalOscillator(channel: number, freqHz: number): void {
    this.client.send(Command(this.id, this.cmds['set_local_oscillator'], channel, freqHz));
  }

  setTrackingEnabled(enabled: boolean): void {
    this.client.send(Command(this.id, this.cmds['set_tracking_enabled'], enabled));
  }

  async getTrackingParameters(): Promise<ITrackingParameters> {
    const [tracking_enabled, tracking_bandwidth, effective_tracking_bandwidth,
      tracking_correction_x, tracking_correction_y, tracking_last_mean_dphi,
      tracking_last_error, tracking_locked] =
      await this.client.readTuple<TupleGetTrackingParameters>(
        Command(this.id, this.cmds['get_tracking_parameters']),
        '?ffffff?'
      );

    return {
      tracking_enabled,
      tracking_bandwidth,
      effective_tracking_bandwidth,
      tracking_correction_x,
      tracking_correction_y,
      tracking_last_mean_dphi,
      tracking_last_error,
      tracking_locked
    };
  }

  async getPhaseNoise(): Promise<Float32Array> {
    return await this.client.readFloat32Array(Command(this.id, this.cmds['get_phase_noise']));
  }

  setMinFrequency(minFrequencyHz: number): void {
    this.client.send(Command(this.id, this.cmds['set_min_frequency'], minFrequencyHz));
  }

  setChannel(channel: number): void {
    this.client.send(Command(this.id, this.cmds['set_channel'], channel));
  }

  resetCumulativeAverager(): void {
    this.client.send(Command(this.id, this.cmds['reset_cumulative_averager']));
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
