// FFT widget
// (c) Koheron

class FFTApp {
    private channelNum: number = 2;
    private fftSelects: HTMLSelectElement[];
    private fftInputs: HTMLInputElement[];

    constructor(document: Document, private driver) {
        this.fftSelects = <HTMLSelectElement[]><any>document.getElementsByClassName("fft-select");
        this.initFFTSelects();
        this.fftInputs = <HTMLInputElement[]><any>document.getElementsByClassName("fft-input");
        this.initFFTInputs();

        this.updateControls();
    }

    // Updaters
    private _busyControls = false;
    private _controlsHz = 10;            // throttle UI refresh rate
    private _lastControlsTick = 0;
    private _ddsInputsByChannel?: HTMLInputElement[][];
    private _supplySpans?: HTMLSpanElement[];
    private _temperatureSpans?: HTMLSpanElement[];

    // Build & cache DOM references once
    private ensureControlsCache() {
        if (!this._ddsInputsByChannel) {
            const all = Array.from(document.querySelectorAll<HTMLInputElement>(
                ".dds-channel-input[data-command='setDDSFreq']"
            ));

            const byChan: Record<string, HTMLInputElement[]> = {};

            for (const el of all) {
                const ch = el.dataset.channel!;
                (byChan[ch] ||= []).push(el);
            }

            const maxChan = Math.max(...Object.keys(byChan).map(Number), this.channelNum - 1);
            this._ddsInputsByChannel = Array.from({ length: maxChan + 1 }, (_, i) => byChan[String(i)] || []);
        }

        if (!this._supplySpans) {
            this._supplySpans = Array.from(document.getElementsByClassName("supply-span")) as HTMLSpanElement[];
        }

        if (!this._temperatureSpans) {
            this._temperatureSpans = Array.from(document.getElementsByClassName("temperature-span")) as HTMLSpanElement[];
        }
    }

    private setCheckedIfNeeded(sel: string) {
      const input = document.querySelector<HTMLInputElement>(sel);
      if (input && !input.checked) input.checked = true;
    }

    private setValueIfNeeded(el: HTMLInputElement | HTMLSelectElement, v: string) {
      if (el.value !== v) el.value = v;
    }

    private setTextIfNeeded(el: HTMLElement, v: string) {
      if (el.textContent !== v) el.textContent = v;
    }

    private setRangeMaxIfNeeded(el: HTMLInputElement, v: string) {
      if (el.type === "range" && el.max !== v) el.max = v;
    }

    private async updateControls() {
        // prevent overlap
        if (this._busyControls) return;
        this._busyControls = true;

        // throttle
        const frameBudgetMs = 1000 / this._controlsHz;
        const now = performance.now();
        const sinceLast = now - this._lastControlsTick;

        if (sinceLast < frameBudgetMs) {
            this._busyControls = false;
            const wait = Math.ceil(frameBudgetMs - sinceLast);
            setTimeout(() => requestAnimationFrame(() => this.updateControls()), wait);
            return;
        }

        this._lastControlsTick = now;

        try {
            this.ensureControlsCache();

            const [sts, brdParams] = await Promise.all([
                this.driver.getControlParameters() as Promise<IFFTStatus>,
                this.driver.getBoardParameters() as Promise<IBoardParameters>,
            ]);

            // Update DDS inputs per channel, but skip the channel if any of its inputs is focused
            const active = document.activeElement as HTMLElement | null;

            for (let ch = 0; ch < this.channelNum; ch++) {
                const inputs = this._ddsInputsByChannel![ch] || [];
                if (!inputs.length) continue;

                // If the active element is one of this channel's inputs, skip updating this channel
                if (active && inputs.includes(active as HTMLInputElement)) continue;

                const freqMHz = (sts.dds_freq[ch] / 1e6).toFixed(6);
                const maxMHz = (sts.fs / 1e6 / 2).toFixed(1);

                for (const inp of inputs) {
                    this.setValueIfNeeded(inp, freqMHz);
                    this.setRangeMaxIfNeeded(inp, maxMHz);
                }
            }

            // Sampling frequency radio
            this.setCheckedIfNeeded(
                sts.fs === 200e6
                    ? "[data-command='setSamplingFrequency'][value='0']"
                    : "[data-command='setSamplingFrequency'][value='1']"
            );

            // Input channel radio
            this.setCheckedIfNeeded(
                `[data-command='setInputChannel'][value='${sts.channel}']`
            );

            // FFT window select
            const winSel = document.querySelector<HTMLSelectElement>("[data-command='setFFTWindow']");

            if (winSel) {
                this.setValueIfNeeded(winSel, String(sts.window_index));
            }

            // Reference clock radio
            this.setCheckedIfNeeded(
                `[data-command='setReferenceClock'][value='${sts.clkIndex}']`
            );

            for (const span of this._supplySpans) {
                const idx = Number(span.dataset.index || "0");
                const val = brdParams.supplyValues[idx];
                const out =
                    span.dataset.type === "voltage"
                    ? val.toFixed(3)
                    : span.dataset.type === "current"
                    ? (val * 1e3).toFixed(1)
                    : "";
                this.setTextIfNeeded(span, out);
            }

            for (const span of this._temperatureSpans) {
                span.textContent = brdParams.temperatures[parseInt(span.dataset.index)].toFixed(3);
            }

            for (let i: number = 0; i < 4; i++) {
                (<HTMLSpanElement>document.querySelector(".precision-adc-span[data-channel='" + i.toString() + "']")).textContent = (brdParams.adcValues[i] * 1000).toFixed(4);
            }

            for (let i = 0; i < 4; i++) {
                let inputs = <HTMLInputElement[]><any>document.querySelectorAll(".precision-dac-input[data-command='setDac'][data-channel='" + i.toString() + "']");
                let inputsArray = [];
                for (let j = 0; j < inputs.length; j++) {
                    inputsArray.push(inputs[j]);
                }

                if (inputsArray.indexOf(<HTMLInputElement>document.activeElement) == -1) {
                    for (let j = 0; j < inputs.length; j++) {
                      inputs[j].value = (brdParams.dacValues[i] * 1000).toFixed(3).toString();
                    }
                }
            }

            // schedule next tick after work is done; keep throttling stable
            const elapsed = performance.now() - now;
            const delay = Math.max(0, Math.ceil(frameBudgetMs - elapsed));
            this._busyControls = false;
            setTimeout(() => requestAnimationFrame(() => this.updateControls()), delay);
        } catch (err) {
            console.error("updateControls error:", err);
            this._busyControls = false;
            setTimeout(() => requestAnimationFrame(() => this.updateControls()), 500);
        }
    }

    // Setters

    initFFTSelects(): void {
        for (let i = 0; i < this.fftSelects.length; i++) {
            this.fftSelects[i].addEventListener('change', (event) => {
                this.driver[(<HTMLSelectElement>event.currentTarget).dataset.command]((<HTMLSelectElement>event.currentTarget).value);
            })
        }
    }

    initFFTInputs(): void {
        for (let i = 0; i < this.fftInputs.length; i++) {
            this.fftInputs[i].addEventListener('change', (event) => {
                this.driver[(<HTMLInputElement>event.currentTarget).dataset.command]((<HTMLInputElement>event.currentTarget).value);
            })
        }
    }

}