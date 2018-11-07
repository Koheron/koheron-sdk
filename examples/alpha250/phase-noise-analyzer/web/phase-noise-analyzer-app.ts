// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {

  public nPoints: number;

  constructor(document: Document, private driver) {
  }

  init(callback: () => void): void {
    this.driver.getParameters((parameters) => {
      this.nPoints = parameters.data_size;
      callback();
    })
  }
}
