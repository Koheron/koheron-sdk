// Interface for the Phase Noise Analyzer driver
// (c) Koheron

class PhaseNoiseAnalyzerApp {
  private cicRateInput: HTMLInputElement;
  private channelInputs: HTMLInputElement[];
  private carrierPowerSpan: HTMLElement;
  private isEditingCic: boolean;
  public nPoints: number;

  constructor(document: Document, private driver) {}

  async init(callback: () => void): Promise<void> {
    const parameters = await this.driver.getParameters();
    this.nPoints = parameters.data_size;

    this.channelInputs = <HTMLInputElement[]><any>document.getElementsByClassName("channel-input");
    this.carrierPowerSpan = <HTMLElement>document.getElementsByClassName("carrier-power-span")[0];
    this.initCicRateInput();
    this.initChannelInput();
    this.updatePower();
    this.updateControls();
    callback();
  }

  initCicRateInput(): void {
    this.cicRateInput = <HTMLInputElement>document.getElementsByClassName("cic-rate-input")[0];

    this.cicRateInput.addEventListener("focus", () => {
      this.isEditingCic = true;
    });

    this.cicRateInput.addEventListener("blur", () => {
      this.isEditingCic = false;
      this.updateControls();
    });

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
    let navg: number = 400;
    this.driver.getCarrierPower(navg, (power: number) => {
      this.carrierPowerSpan.innerHTML = power.toFixed(2) + " dBm";
      requestAnimationFrame( () => { this.updatePower(); } )
    });
  }

  private async updateControls(): Promise<void> {
    const parameters = await this.driver.getParameters();

    if (parameters.channel == 0) {
      this.channelInputs[0].checked = true;
      this.channelInputs[1].checked = false;
    } else {
      this.channelInputs[0].checked = false;
      this.channelInputs[1].checked = true;
    }

    if (!this.isEditingCic) {
      this.cicRateInput.value = parameters.cic_rate.toString();
    }

    requestAnimationFrame( () => { this.updateControls(); } )
  }
}
