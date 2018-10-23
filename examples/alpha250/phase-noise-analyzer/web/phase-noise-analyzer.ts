// Interface for the Phase Noise Analyzer driver
// (c) Koheron

interface IParameters {
  data_size: number; // fft_size/2
  fs: number; // Sampling frequency (Hz)
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

  start(): void {
    this.client.send(Command(this.id, this.cmds['start']));
  }


  getParameters(callback: (parameters: IParameters) => void): void {
    this.client.readTuple(Command(this.id, this.cmds['get_parameters']), 'If',
    (tup: [number, number]) => {
      this.parameters.data_size = tup[0];
      this.parameters.fs = tup[1];
      callback(this.parameters);
    });
  }


}
