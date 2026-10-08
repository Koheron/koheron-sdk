// FFT widget
// (c) Koheron

class FFTApp {
    private running: boolean = true;
    private events = new InstrumentEvents();
    private channelNum: number = 2;
    private fftSelects: HTMLSelectElement[];
    private fftInputs: HTMLInputElement[];
    private onChange = (event: Event): void => {
        if (!this.running) { return; }
        const input = event.currentTarget as HTMLInputElement | HTMLSelectElement;
        this.driver[input.dataset.command](Number(input.value));
    };

    constructor(private document: Document, private driver, private samplingRateChanged?: (rate: number) => void,
                private precisionDacChanged?: (values: ArrayLike<number>) => void) {
        this.fftSelects = <HTMLSelectElement[]><any>this.document.getElementsByClassName("fft-select");
        this.initFFTSelects();
        this.fftInputs = <HTMLInputElement[]><any>this.document.getElementsByClassName("fft-input");
        this.initFFTInputs();

        this.updateControls();
        if (typeof this.driver.getBoardParameters === 'function') {
            this.boardPoller = new InstrumentPoller(this.document,
                () => this.driver.getBoardParameters(), values => this.updateBoard(values));
            this.boardPoller.start();
        }
    }

    // Updaters
    private _busyControls = false;
    private _controlsHz = 4;            // throttle UI refresh rate
    private _lastControlsTick = 0;
    private boardPoller?: InstrumentPoller<IBoardParameters>;
    private _ddsInputsByChannel?: HTMLInputElement[][];
    private _supplySpans?: HTMLSpanElement[];
    private _temperatureSpans?: HTMLSpanElement[];

    // Build & cache DOM references once
    private ensureControlsCache() {
        if (!this._ddsInputsByChannel) {
            const all = Array.from(this.document.querySelectorAll<HTMLInputElement>(
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
            this._supplySpans = Array.from(this.document.getElementsByClassName("supply-span")) as HTMLSpanElement[];
        }

        if (!this._temperatureSpans) {
            this._temperatureSpans = Array.from(this.document.getElementsByClassName("temperature-span")) as HTMLSpanElement[];
        }
    }

    private setCheckedIfNeeded(sel: string) {
      const input = this.document.querySelector<HTMLInputElement>(sel);
      if (input && !input.checked) input.checked = true;
    }

    private setValueIfNeeded(el: HTMLInputElement | HTMLSelectElement, v: string) {
      if (el.value !== v) el.value = v;
    }

    private setMaxIfNeeded(el: HTMLInputElement, v: string) {
      if (el.max !== v) el.max = v;
    }

    private async updateControls() {
        if (!this.running) { return; }
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
            setTimeout(() => { if (this.running) { this.updateControls(); } }, wait);
            return;
        }

        this._lastControlsTick = now;

        try {
            this.ensureControlsCache();

            const sts: IFFTStatus = await this.driver.getControlParameters();
            if (!this.running) { this._busyControls = false; return; }
            if (this.samplingRateChanged) { this.samplingRateChanged(sts.fs); }

            // Update DDS inputs per channel, but skip the channel if any of its inputs is focused
            const active = this.document.activeElement as HTMLElement | null;

            for (let ch = 0; ch < this.channelNum; ch++) {
                const inputs = this._ddsInputsByChannel![ch] || [];
                if (!inputs.length) continue;

                const maxMHz = (sts.fs / 1e6 / 2).toFixed(1);
                for (const inp of inputs) { this.setMaxIfNeeded(inp, maxMHz); }

                // Keep an edit intact, but always refresh its hardware limit.
                if (active && inputs.includes(active as HTMLInputElement)) continue;

                const freqMHz = (sts.dds_freq[ch] / 1e6).toFixed(6);
                for (const inp of inputs) { this.setValueIfNeeded(inp, freqMHz); }
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
            const winSel = this.document.querySelector<HTMLSelectElement>("[data-command='setFFTWindow']");

            if (winSel && this.document.activeElement !== winSel) {
                this.setValueIfNeeded(winSel, String(sts.window_index));
            }

            // Reference clock radio
            this.setCheckedIfNeeded(
                `[data-command='setReferenceClock'][value='${sts.clkIndex}']`
            );

            // schedule next tick after work is done; keep throttling stable
            const elapsed = performance.now() - now;
            const delay = Math.max(0, Math.ceil(frameBudgetMs - elapsed));
            this._busyControls = false;
            setTimeout(() => { if (this.running) { this.updateControls(); } }, delay);
        } catch (err) {
            this._busyControls = false;
            if (!this.running) { return; }
            console.error("updateControls error:", err);
            setTimeout(() => { if (this.running) { this.updateControls(); } }, 500);
        }
    }

    private updateBoard(brdParams: IBoardParameters): void {
        this.ensureControlsCache();
        updateSupplyReadouts(this._supplySpans, brdParams.supplyValues);
        updateTemperatureReadouts(this._temperatureSpans, brdParams.temperatures);

        for (let i: number = 0; i < 4; i++) {
            (<HTMLSpanElement>this.document.querySelector(".precision-adc-span[data-channel='" + i.toString() + "']")).textContent = (brdParams.adcValues[i] * 1000).toFixed(4);
        }

        if (this.precisionDacChanged) { this.precisionDacChanged(brdParams.dacValues); }
        else for (let i = 0; i < 4; i++) {
            let inputs = <HTMLInputElement[]><any>this.document.querySelectorAll(".precision-dac-input[data-command='setDac'][data-channel='" + i.toString() + "']");
            let inputsArray = [];
            for (let j = 0; j < inputs.length; j++) {
                inputsArray.push(inputs[j]);
            }

            if (inputsArray.indexOf(<HTMLInputElement>this.document.activeElement) == -1) {
                for (let j = 0; j < inputs.length; j++) {
                  inputs[j].value = (brdParams.dacValues[i] * 1000).toFixed(3).toString();
                }
            }
        }
    }

    // Setters

    initFFTSelects(): void {
        for (const input of Array.from(this.fftSelects)) {
            this.events.listen(input, 'change', this.onChange);
        }
    }

    initFFTInputs(): void {
        for (const input of Array.from(this.fftInputs)) {
            this.events.listen(input, 'change', this.onChange);
        }
    }

    dispose(): void {
        this.running = false;
        this.boardPoller?.dispose();
        this.events?.dispose();
    }

}
