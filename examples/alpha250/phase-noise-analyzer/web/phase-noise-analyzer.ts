// Interface for the Phase Noise Analyzer driver
// (c) Koheron

type TupleGetParameters = [number, number, number, number, number];

interface IParameters {
  data_size: number; // fft_size/2
  fs: number;        // Sampling frequency (Hz)
  channel: number;   // Acquired channel
  cic_rate: number;
  fft_navg: number;
}

type TupleGetJitter = [number, number, number, number];

interface IJitter {
  phase_jitter: number; // rad rms
  time_jitter: number;  // s rms
  freq_lo: number; // Integration interval start
  freq_hi: number; // Integration interval end
}

class PhaseNoiseAnalyzer {
  private driver: Driver;
  private id: number;
  private cmds: Commands;

  constructor(private client: Client) {
    this.driver = this.client.getDriver('PhaseNoiseAnalyzer');
    this.id = this.driver.id;
    this.cmds = this.driver.getCmds();
  }

  getData(callback: (data: Uint32Array) => void): void {
    this.client.readUint32Array(Command(this.id, this.cmds['get_data']), (data: Uint32Array) => {
      callback(data);
    });
  }

  getParameters(): Promise<IParameters> {
    return new Promise<IParameters>((resolve, reject) => {
      this.client.readTuple(
        Command(this.id, this.cmds['get_parameters']),
        'IfIII',
        (tup: TupleGetParameters) => {
          const [data_size, fs, channel, cic_rate, fft_navg] = tup;
          const params: IParameters = { data_size, fs, channel, cic_rate, fft_navg };
          resolve(params);
        });
    });
  }

  getJitter(): Promise<IJitter> {
    return new Promise<IJitter>((resolve, reject) => {
      this.client.readTuple(
        Command(this.id, this.cmds['get_jitter']),
        'ffff',
        (tup: TupleGetJitter) => {
          const [phase_jitter, time_jitter, freq_lo, freq_hi] = tup;
          const jitters: IJitter = { phase_jitter, time_jitter, freq_lo, freq_hi };
          resolve(jitters);
        });
    });
  }

  setFFTNavg(navg: number): void {
    this.client.send(Command(this.id, this.cmds['set_fft_navg'], navg));
  }

  setLocalOscillator(channel: number, freqHz: number): void {
    this.client.send(Command(this.id, this.cmds['set_local_oscillator'], channel, freqHz));
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
    this.client.readFloat64(Command(this.id, this.cmds['get_carrier_power'], nAverage), (data: number) => {
      callback(data);
    });
  }
}
