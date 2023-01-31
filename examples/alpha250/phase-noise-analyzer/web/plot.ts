// Plot widget
// (c) Koheron

class Plot {
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  private nAverage: number;
  private nAvgInput: HTMLInputElement;
  private samplingFrequency: number;

  public yLabel: string = "PHASE NOISE (dBc/Hz)";
  private peakDatapoint: number[];

  constructor(document: Document, private driver: PhaseNoiseAnalyzer, private plotBasics: PlotBasics) {
    this.peakDatapoint = [];
    this.plot_data = [];
    this.init();
    this.nAverage = 1;
    this.nAvgInput = <HTMLInputElement>document.getElementsByClassName("plot-navg-input")[0];
    this.initNavgInput();
    this.updatePlot();
  }

  init() {
    this.driver.start(() => {
      this.driver.getParameters((parameters) => {
        this.n_pts = parameters.data_size;
        this.samplingFrequency = parameters.fs;
        this.setFreqAxis();
        this.plotBasics.setLogX();
      });
    });
  }

  initNavgInput(): void {
    let events = ['change', 'input'];
    for (let j = 0; j < events.length; j++) {
      this.nAvgInput.addEventListener(events[j], (event) => {
          let value = parseInt((<HTMLInputElement>event.currentTarget).value);
          this.setNavg(value);
      });
    }
  }

  setNavg(navg: number) {
    this.nAverage = navg;
  }

  setFreqAxis(): void {
    this.plotBasics.x_min = this.samplingFrequency / this.n_pts;
    this.plotBasics.x_max = 0.75 * 0.5 * this.samplingFrequency;
    this.plotBasics.setRangeX(this.plotBasics.x_min, this.plotBasics.x_max);
  }

  updatePlot() {
    this.driver.getPhaseNoise(this.nAverage, (data: Float32Array) => {
      this.driver.getParameters((parameters) => {
        if (parameters.fs != this.samplingFrequency) {
          this.samplingFrequency = parameters.fs;
          this.setFreqAxis();
        }

        for (let i = 0; i < this.n_pts; i++) {
          let x: number = i * this.samplingFrequency / this.n_pts / 2;
          this.plot_data[i] = [x, 10 * Math.log10(data[i])]
        }

        this.plotBasics.redraw(this.plot_data, this.n_pts, this.peakDatapoint, this.yLabel, () => {
          requestAnimationFrame(() => { this.updatePlot(); });
        });
      });
    });
  }
}
