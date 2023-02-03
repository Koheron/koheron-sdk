// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {
  private cicRateInput: HTMLInputElement;
  private channelInputs: HTMLInputElement[];
  private carrierPowerSpan: HTMLElement;
  public nPoints: number;

  constructor(document: Document, private driver) {}

  init(callback: () => void): void {
    this.driver.getParameters((parameters) => {
      this.nPoints = parameters.data_size;
      this.cicRateInput = <HTMLInputElement>document.getElementsByClassName("cic-rate-input")[0];
      this.channelInputs = <HTMLInputElement[]><any>document.getElementsByClassName("channel-input");
      this.carrierPowerSpan = <HTMLElement>document.getElementsByClassName("carrier-power-span")[0];
      this.initCicRateInput();
      this.initChannelInput();
      this.updatePower();
      this.updateControls();
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

  private updatePower() {
    let navg: number = 100;
    this.driver.getCarrierPower(navg, (power) => {
      this.carrierPowerSpan.innerHTML = power.toFixed(2) + " dBm";
      requestAnimationFrame( () => { this.updatePower(); } )
    });
  }

  private updateControls() {
    this.driver.getParameters( (parameters: IParameters) => {
      if (parameters.channel == 0) {
        this.channelInputs[0].checked = true;
        this.channelInputs[1].checked = false;
      } else {
        this.channelInputs[0].checked = false;
        this.channelInputs[1].checked = true;
      }

      this.cicRateInput.value = parameters.cic_rate.toString();

      requestAnimationFrame( () => { this.updateControls(); } )
    });
  }
}
