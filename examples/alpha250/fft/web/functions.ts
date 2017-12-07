class Functions {
    private isFunctions: boolean = false;
    private functionsSpan: HTMLSpanElement;

    constructor(document: Document) {
        this.functionsSpan = <HTMLSpanElement>document.getElementById("functions");
    }

    updateFunctionsSpan(event) {
        let rect = event.getBoundingClientRect();

        this.functionsSpan.style.left = (rect.x).toString() + "px";
        this.functionsSpan.style.top = (rect.y + rect.height + 10).toString() + "px";
        this.functionsSpan.style.zIndex = "1";
        this.functionsSpan.style.display = "inline-block";
    }

    activateFunctions() {
        if (this.isFunctions) {
            this.isFunctions = false;
            this.hideFunctions();
        } else {
            this.isFunctions = true;
        }
    }

    displayFunctions(event) {
        let pythonFunction: string = "";
        let pythonClass: string = "";

        if (this.isFunctions) {
          if (event.id === "input-channel-label") {
              pythonFunction = "set_input_channel(self, channel)";
              pythonClass = "FFT";
          } else if (event.id === "reference-clock-label") {
              pythonFunction = "set_reference_clock(self, clkin)";
              pythonClass = "Alpha";
          } else if (event.id === "sampling-frequency-label") {
              pythonFunction = "set_sampling_frequency(self, fs_select)";
              pythonClass = "Alpha";
          } else if (event.id === "dds-frequency-span") {
              pythonFunction = "set_dds_freq(self, channel, freq)";
              pythonClass = "FFT";
          } else if (event.id === "precision-dacs-span") {
              pythonFunction = "set_precision_dac_volts(self, channel, voltage)";
              pythonClass = "Alpha";
          } else if (event.id === "precision-adcs-span") {
              pythonFunction = "get_precision_adc_values(self)";
              pythonClass = "Alpha";
          } else if (event.id === "temperature-span") {
              pythonFunction = "get_temperatures(self)";
              pythonClass = "Alpha";
          } else if (event.id === "fft-window-label") {
              pythonFunction = "set_fft_window(self, window_name)";
              pythonClass = "FFT";
          } else if (event.id === "psd-label") {
              pythonFunction = "read_psd(self)";
              pythonClass = "FFT";
          }

          this.functionsSpan.innerHTML = pythonFunction + ", class: " + pythonClass;
          this.updateFunctionsSpan(event);
        }
    }

    hideFunctions() {
        this.functionsSpan.style.display = "none";
    }
}