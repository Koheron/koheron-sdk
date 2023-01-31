// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {
  private cicRateInput: HTMLInputElement;
  private channelInputs: HTMLInputElement[];
  public nPoints: number;

  constructor(document: Document, private driver) {}

  init(callback: () => void): void {
    this.driver.getParameters((parameters) => {
      this.nPoints = parameters.data_size;
      this.cicRateInput = <HTMLInputElement>document.getElementsByClassName("cic-rate-input")[0];
      this.channelInputs = <HTMLInputElement[]><any>document.getElementsByClassName("channel-input");
      this.initCicRateInput();
      this.initChannelInput();
      callback();
    });
  }

  initCicRateInput(): void {
    let events = ['change', 'input'];
    for (let j = 0; j < events.length; j++) {
      this.cicRateInput.addEventListener(events[j], (event) => {
          let command = (<HTMLInputElement>event.currentTarget).dataset.command;
          let value = (<HTMLInputElement>event.currentTarget).value;
          this.driver[command](value);
      });
    }
  }

  initChannelInput(): void {
    for (let i = 0; i < this.channelInputs.length; i++) {
      this.channelInputs[i].addEventListener('change', (event) => {
          this.driver[(<HTMLInputElement>event.currentTarget).dataset.command](parseInt((<HTMLInputElement>event.currentTarget).value));
      })
    }
  }
}
