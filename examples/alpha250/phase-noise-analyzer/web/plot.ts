// Plot widget
// (c) Koheron

class Plot {
  public n_pts: number;
  public plot: jquery.flot.plot;
  public plot_data: Array<Array<number>>;
  public decade_values: Array<Array<number>>;
  private nAverage: number;
  private nAvgInput: HTMLInputElement;
  private samplingFrequency: number;
  private decadeValuesTable: HTMLTableElement;

  public yLabel: string = "PHASE NOISE (dBc/Hz)";
  private peakDatapoint: number[];

  constructor(document: Document, private driver: PhaseNoiseAnalyzer, private plotBasics: PlotBasics) {
    this.peakDatapoint = [];
    this.plot_data = [];
    this.init();
    this.nAverage = 1;
    this.nAvgInput = <HTMLInputElement>document.getElementsByClassName("plot-navg-input")[0];
    this.decadeValuesTable = <HTMLTableElement>document.getElementById('decade-values-table');
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

  frequencyFormater(val: number) {
    if (val >= 1E6) {
      return (val / 1E6).toString() + " MHz";
    } else if (val >= 1E3) {
        return (val / 1E3).toFixed() + " kHz";
    } else {
        return val.toFixed() + " Hz";
    }
  }

  getDecadeValues() {
    let fmin: number = this.plotBasics.x_min;
    let fmax: number = this.plotBasics.x_max;

    let freq_decades: number[] = [1E-1, 1E0, 1E1, 1E2, 1E3, 1E4, 1E5, 1E6, 1E7];
    this.decade_values = [];

    for (let freq of freq_decades) {
      if (freq >= fmin && freq <= fmax) {
        let i: number = Math.floor(2 * freq * this.n_pts / this.samplingFrequency);
        this.decade_values.push(this.plot_data[i]);
      }
    }

    this.decadeValuesTable.innerHTML = '<thead><tr><th>Carrier Offset Frequency</th><th>Phase Noise</th></tr></thead>';

    for (let value of this.decade_values) {
      let row = this.decadeValuesTable.insertRow(-1);
      let freqCell = row.insertCell(0);
      freqCell.innerHTML = this.frequencyFormater(value[0]);
      let valueCell = row.insertCell(1);
      valueCell.innerHTML = value[1].toFixed(2) + " dBc/Hz";
    }
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

        this.getDecadeValues();

        this.plotBasics.redraw(this.plot_data, this.n_pts, this.peakDatapoint, this.yLabel, () => {
          requestAnimationFrame(() => { this.updatePlot(); });
        });
      });
    });
  }
}
