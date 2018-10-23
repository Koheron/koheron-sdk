// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {

  constructor(document: Document, private driver) {
    this.driver.start();
    this.updateParameters();
  }

  private updateParameters() {
    this.driver.getParameters( (parameters: IParameters) => {
      console.log(parameters.data_size);
      console.log(parameters.fs);
    });
  }
}
