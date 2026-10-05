class Alpha15SignalAnalyzerControls implements FFTBoardControls {
    private disposed = false;
    private timer: number;
    private precision: PrecisionChannelsApp;
    private dac: PrecisionDac;
    private ranges: Ltc2387;
    private clock: ClockGenerator;
    private temperature: TemperatureSensor;
    private power: PowerMonitor;

    constructor(private client: Client, private document: Document, private settingsChanged: () => void) {}

    async init(): Promise<void> {
        this.ranges = new Ltc2387(this.client);
        this.clock = new ClockGenerator(this.client);
        this.dac = new PrecisionDac(this.client);
        this.temperature = new TemperatureSensor(this.client);
        this.power = new PowerMonitor(this.client);
        this.precision = new PrecisionChannelsApp(this.document, this.dac);
        for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('.adc-range'))) {
            input.addEventListener('change', () => {
                this.settingsChanged();
                this.ranges.setInputRange(Number(input.value));
            });
        }
        for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('.clkgen-input'))) {
            input.addEventListener('change', () => {
                this.settingsChanged();
                this.clock.setReferenceClock(Number(input.value));
            });
        }
        await this.precision.init();
        if (this.disposed) { return; }
        for (const id of ['input-range', 'reference-clock', 'board-acquisition-note']) { this.document.getElementById(id).hidden = false; }
        this.document.getElementById('board-acquisition-note').textContent = '15 MS/s · LF + mid + RF';
        const details = this.document.querySelector<HTMLDetailsElement>('.board-details');
        details.hidden = false;
        details.open = true;
        (this.document.getElementById('board-controls') as HTMLFieldSetElement).disabled = false;
        void this.poll();
    }

    private async poll(): Promise<void> {
        if (this.disposed) { return; }
        try {
            if (!this.document.hidden) {
                const [range0, range1, dac, temperatures, supplies] = await Promise.all([
                    this.ranges.inputRange(0), this.ranges.inputRange(1), this.dac.getDacValues(),
                    this.temperature.getTemperatures(), this.power.getSuppliesUI()
                ]);
                if (this.disposed) { return; }
                for (const [channel, range] of Array.from([range0, range1].entries())) {
                    this.document.querySelector<HTMLInputElement>(`.adc-range[value='${channel * 2 + range}']`).checked = true;
                }
                const selected = this.document.querySelector<HTMLInputElement>("[data-command='setInputChannel']:checked");
                this.document.getElementById('range-warning').hidden = range0 === range1 || Number(selected?.value) < 2;
                this.precision.setValues(dac);
                for (const span of Array.from(this.document.querySelectorAll<HTMLElement>('.temperature-span'))) {
                    span.textContent = temperatures[Number(span.dataset.index)].toFixed(1);
                }
                for (const span of Array.from(this.document.querySelectorAll<HTMLElement>('.supply-span'))) {
                    const value = supplies[Number(span.dataset.index)];
                    span.textContent = span.dataset.type === 'voltage' ? value.toFixed(3) : (value * 1000).toFixed(1);
                }
                this.document.querySelector('.board-details summary').removeAttribute('title');
            }
        } catch (error) {
            if (!this.disposed) {
                this.document.querySelector('.board-details summary').setAttribute('title', 'Board readback unavailable; retrying…');
                console.error('Alpha15 board readback failed:', error);
            }
        } finally {
            if (!this.disposed) { this.timer = window.setTimeout(() => this.poll(), 1000); }
        }
    }

    dispose(): void {
        this.disposed = true;
        window.clearTimeout(this.timer);
        this.precision?.dispose();
    }
}
