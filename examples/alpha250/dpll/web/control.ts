class Control {

  private channelButtons: any;
  private frequencyInput: HTMLInputElement;
  private frequencyButton: HTMLButtonElement;
  private gainPowerMin: number;
  private gainPowerMax: number;
  private gainButtons: any;
  private gainInputs: any;
  private gainSaveButtons: any;
  private integratorSwitches: any;
  private dacOutputSelects: any;

  constructor(document: Document, private dpll: Dpll) {

    this.channelButtons = <HTMLButtonElement[]><any>document.getElementsByClassName("channel-button");

    this.frequencyInput = <HTMLInputElement>document.getElementsByClassName("frequency-input")[0];
    this.frequencyButton = <HTMLButtonElement>document.getElementsByClassName("frequency-save")[0];
    this.integratorSwitches = <HTMLInputElement[]><any>document.getElementsByClassName("integrator-switch");

    this.gainPowerMin = 0;
    this.gainPowerMax = 30.99;

    this.gainButtons = <HTMLButtonElement[]><any>document.getElementsByClassName("gain-button");
    this.gainInputs = <HTMLInputElement[]><any>document.getElementsByClassName("gain-input");
    this.gainSaveButtons = <HTMLButtonElement[]><any>document.getElementsByClassName("gain-save");

    for (let i = 0; i < this.gainInputs.length; i++) {
      this.gainInputs[i].min = this.gainPowerMin;
      this.gainInputs[i].max = this.gainPowerMax;
      this.gainInputs[i].step = 0.01;
    }

    this.dacOutputSelects = <HTMLSelectElement[]><any>document.getElementsByClassName("dac-output");

    this.setChannel();
    this.updateFrequencies();
    this.saveFrequencies();
    this.setIntegrators();
    this.updateGainInputs();
    this.updateGainFactors();
    this.saveGains();
    this.setDacOutputs();
    this.updateStatus();
  }

  updateStatus() {
    this.dpll.getControlParameters( (sts: IDpllStatus) => {

      for (let key in sts) {

        if (sts.hasOwnProperty(key)) {

          let currentChannel: number = 0;

          for (let i = 0; i < this.channelButtons.length; i++) {
            if (this.channelButtons[i].classList.contains("active")) {
              currentChannel = i;
            }
          }

          if ( key == "dds_freq" ) {

            let input = <HTMLInputElement>document.querySelector(".status-input[data-status='" + key + "'][data-channel='" + currentChannel.toString() + "']");
            let button = <HTMLButtonElement>document.querySelector(".frequency-save[data-channel='" + currentChannel.toString() + "']");
            if ( button.classList.contains("disabled") ) {
              input.value = (sts[key][currentChannel] / 1e6).toString();
            }

          } else if ( key.indexOf("gain") > -1 ) {

              let input = <HTMLInputElement>document.querySelector(".status-input[data-status='" + key + "'][data-channel='" + currentChannel.toString() + "']");
              let buttons = <HTMLButtonElement[]><any>document.querySelectorAll(".status-button[data-status='" + key + "'][data-channel='" + currentChannel.toString() + "']");
              let value: string = "";
              let saveButton = <HTMLButtonElement>document.querySelector(".gain-save[data-status='" + key + "'][data-channel='" + currentChannel.toString() + "']");

              let buttonsArray = [];
              for (let j = 0; j < buttons.length; j++) {
                buttonsArray.push(buttons[j]);
              }

              if ( saveButton.classList.contains("disabled") ) {

                if (sts[key][currentChannel] === 0) {
                  value = "0";
                } else if (sts[key][currentChannel] > 0){
                    value = "+1";
                } else if (sts[key][currentChannel] < 0) {
                    value = "-1";
                }

                let button = <HTMLButtonElement>document.querySelector(".status-button[data-status='" + key + "'][data-channel='" + currentChannel.toString() + "'][value='" + value + "']");
                button.click();

                if (sts[key][currentChannel] !== 0) {
                  input.value = (Math.round(Math.log(Math.abs(sts[key][currentChannel])) / Math.log(2) * 100)/100.0).toString();
                }

                saveButton.classList.add("disabled");
              }

            } else if (key == "integrators") {

            let count: number = 4;
            let status: string = sts[key][currentChannel].toString(2).split("").reverse().join("");
            while (status.length < count) {
              status += '0';
            }

            for (let j = 0; j < status.length; j++) {
              let intOn: boolean = !!+status.charAt(j) ;
              let intSwitch = <HTMLInputElement>document.querySelector(".integrator-switch[data-integratorindex='" + j.toString() + "'][data-channel='" + currentChannel.toString() + "']");
              intSwitch.checked = intOn;
            }
          }


        }
      }

      requestAnimationFrame( () => { this.updateStatus(); } )
    });
  }

  updateFrequencies() {
    let events = ['change', 'keyup'];
    for (let j = 0; j < events.length; j++) {
      this.frequencyInput.addEventListener( events[j], (event) => {
        let channel: number = parseInt((<HTMLButtonElement>event.currentTarget).dataset.channel);
        let button = <HTMLButtonElement>document.querySelector(".frequency-save[data-channel='" + channel + "']");
        button.classList.remove("disabled");
      })
    }
  }

  saveFrequencies() {

    let saveFrequencyButtons = <HTMLButtonElement[]><any>document.querySelectorAll(".frequency-save");
    for (let i = 0; i < saveFrequencyButtons.length ; i++) {
      saveFrequencyButtons[i].addEventListener( 'click', (event) => {
        let channel: number = parseInt((<HTMLButtonElement>event.currentTarget).dataset.channel);
        let command: string = (<HTMLButtonElement>event.currentTarget).dataset.command;
        let value: string = (<HTMLInputElement>document.querySelector(".frequency-input[data-channel='" + channel + "']")).value;
        this.dpll[command](channel, 1e6 * parseFloat(value));
        (<HTMLButtonElement>event.currentTarget).classList.add("disabled");
      })
    }
  }

  updateGainInputs() {

    let events = ['change', 'keyup'];
    for (let i = 0; i < this.gainInputs.length ; i++) {
      for (let j = 0; j < events.length; j++) {
        this.gainInputs[i].addEventListener( events[j], (event) => {
          let channel: number = parseInt((<HTMLButtonElement>event.currentTarget).dataset.channel);
          let status: string = event.currentTarget.dataset.status;
          let saveButton = <HTMLButtonElement>document.querySelector(".gain-save[data-status='" + status + "'][data-channel='" + channel + "']");
          saveButton.classList.remove("disabled");
        })
      }
    }

  }

  updateGainFactors() {

    for ( let i = 0; i < this.gainButtons.length ; i++ ) {

      this.gainButtons[i].addEventListener('click', (event) => {

        let buttons = event.target.parentElement.getElementsByClassName("gain-button");
        for (let i = 0; i < buttons.length; i ++) {
          if (event.target == buttons[i]) {
            buttons[i].classList.add("active");
          } else {
            buttons[i].classList.remove("active");
          }
        }

        let channel: number = parseInt(event.currentTarget.dataset.channel);
        let status: string = event.currentTarget.dataset.status;
        let input = <HTMLInputElement><any>document.querySelector("input[data-status='" + status + "'][data-channel='" + channel + "']");

        if (event.currentTarget.value === "0") {
          input.disabled = true;
          input.style.color = "hsl(0, 0%, 60%)";
          input.value = "";
        } else {
          input.disabled = false;
          input.style.color = "inherit";
        }

        let saveButton = <HTMLButtonElement>document.querySelector(".gain-save[data-status='" + status + "'][data-channel='" + channel + "']");
        saveButton.classList.remove("disabled");

      })
    }

  }

  saveGains() {
    for (let i = 0; i < this.gainSaveButtons.length ; i++) {
      this.gainSaveButtons[i].addEventListener( 'click', (event) => {
        let channel: number = parseInt((<HTMLButtonElement>event.currentTarget).dataset.channel);
        let command: string = (<HTMLButtonElement>event.currentTarget).dataset.command;
        let status: string = (<HTMLButtonElement>event.currentTarget).dataset.status;
        let factorButton = <HTMLButtonElement>document.querySelector(".gain-button.active[data-status='" + status + "'][data-channel='" + channel + "']");
        let factor: number = parseInt(factorButton.value);
        let input = <HTMLInputElement>document.querySelector(".gain-input[data-status='" + status + "'][data-channel='" + channel + "']");
        let gain = Math.round(factor * Math.pow(2, parseFloat(input.value)));

        this.dpll[command](channel, gain);
        (<HTMLButtonElement>event.currentTarget).classList.add("disabled");
      })
    }
  }

  setIntegrators() {

    for (let i = 0; i < this.integratorSwitches.length; i++) {
      this.integratorSwitches[i].addEventListener('change', (event) => {

        let integratorOn:boolean;

        if (this.integratorSwitches[i].checked) {
          integratorOn = true;
        } else {
          integratorOn = false;
        }

        let channel: number = parseInt(this.integratorSwitches[i].dataset.channel);
        let integratorIndex: number = parseInt(this.integratorSwitches[i].dataset.integratorindex);

        this.dpll.setIntegrator(channel, integratorIndex, integratorOn);

      })
    }

  }

  setChannel() {

    for (let i = 0; i < this.channelButtons.length; i++) {
      this.channelButtons[i].addEventListener( 'click', (event) => {

        let currentChannel: string = event.currentTarget.value;
        event.currentTarget.classList.add("active");

        for (let j = 0; j < this.channelButtons.length; j++) {
          if (i !== j) {
            this.channelButtons[j].classList.remove("active");
          }
        }

        this.frequencyInput.dataset.channel = currentChannel;
        this.frequencyButton.dataset.channel = currentChannel;

        for (let k = 0; k < this.integratorSwitches.length; k++) {
          this.integratorSwitches[k].dataset.channel = currentChannel;
        }

        for (let k = 0; k < this.gainButtons.length; k++) {
          this.gainButtons[k].dataset.channel = currentChannel;
        }

        for (let k = 0; k < this.gainInputs.length; k++) {
          this.gainInputs[k].dataset.channel = currentChannel;
        }

        for (let k = 0; k < this.gainSaveButtons.length; k++) {
          this.gainSaveButtons[k].dataset.channel = currentChannel;
        }

        let animationClass: string = "slide-in-right";

        if (currentChannel == "0") {
          animationClass = "slide-in-right";
        } else if (currentChannel == "1") {
          animationClass = "slide-in-left";
        }

        document.getElementById("channel-parameters").classList.add(animationClass);
        setTimeout(() => {
          document.getElementById("channel-parameters").classList.remove(animationClass);
        }, 500)

      })
    }

  }

  setDacOutputs() {
    for (let i = 0; i < this.dacOutputSelects.length; i++) {
      this.dacOutputSelects[i].addEventListener('change', (event) => {
        let channel: number = parseInt(event.currentTarget.dataset.channel);
        let sel: number = parseInt(event.currentTarget.value);
        this.dpll.setDacOutput(channel, sel);
      })
    }
  }

}