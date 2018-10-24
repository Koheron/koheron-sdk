// Plot widget
// (c) Koheron

class Plot {
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  private nAverage: number;
  private samplingFrequency: number;

  public yLabel: string = "PHASE NOISE (dBc/Hz)";
  private peakDatapoint: number[];

  constructor(document: Document, private driver: PhaseNoiseAnalyzer, private plotBasics: PlotBasics) {
    this.peakDatapoint = [];
    this.plot_data = [];
    this.init();
    this.nAverage = 1;
    this.updatePlot();
  }

  init() {
    this.driver.start(() => {
      this.driver.getParameters((parameters) => {
        this.n_pts = parameters.data_size;
        this.samplingFrequency = parameters.fs;
      })
    });
  }

  updatePlot() {
    this.driver.getPhaseNoise(this.nAverage, (data: Float32Array) => {
      for (let i = 0; i < this.n_pts; i++) {
        let x: number = i * this.samplingFrequency / this.n_pts / 2;
        this.plot_data[i] = [x, 10 * Math.log10(data[i])]
      }
      this.plotBasics.redraw(this.plot_data, this.n_pts, this.peakDatapoint, this.yLabel, () => {
        requestAnimationFrame(() => { this.updatePlot(); });
      });
    });
  }

}
