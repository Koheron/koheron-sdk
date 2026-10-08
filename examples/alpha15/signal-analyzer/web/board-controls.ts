class Alpha15SignalAnalyzerControls implements FFTBoardControls {
    private disposed = false;
    private events = new InstrumentEvents();
    private telemetry: InstrumentPoller<[number, number, Float32Array, Float32Array, Float32Array]>;
    private precision: PrecisionChannelsApp;
    private dac: PrecisionDac;
    private ranges: Ltc2387;
    private clock: ClockGenerator;
    private clockControls: ClockGeneratorApp;
    private temperature: TemperatureSensor;
    private power: PowerMonitor;

    constructor(private client: Client, private document: Document, private settingsChanged: () => void) {}

    async init(): Promise<void> {
        this.ranges = new Ltc2387(this.client);
        this.clock = new ClockGenerator(this.client);
        this.clockControls = new ClockGeneratorApp(this.document, this.clock, this.settingsChanged);
        this.dac = new PrecisionDac(this.client);
        this.temperature = new TemperatureSensor(this.client);
        this.power = new PowerMonitor(this.client);
        this.precision = new PrecisionChannelsApp(this.document, this.dac);
        for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('.adc-range'))) {
            this.events.listen(input, 'change', () => {
                this.settingsChanged();
                this.ranges.setInputRange(Number(input.value));
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
        this.telemetry = new InstrumentPoller(this.document,
            () => Promise.all([
                this.ranges.inputRange(0), this.ranges.inputRange(1), this.dac.getDacValues(),
                this.temperature.getTemperatures(), this.power.getSuppliesUI()
            ]),
            ([range0, range1, dac, temperatures, supplies]) => {
                for (const [channel, range] of Array.from([range0, range1].entries())) {
                    this.document.querySelector<HTMLInputElement>(`.adc-range[value='${channel * 2 + range}']`).checked = true;
                }
                const selected = this.document.querySelector<HTMLInputElement>("[data-command='setInputChannel']:checked");
                this.document.getElementById('range-warning').hidden = range0 === range1 || Number(selected?.value) < 2;
                this.precision.setValues(dac);
                updateTemperatureReadouts(this.document.querySelectorAll<HTMLElement>('.temperature-span'), temperatures);
                updateSupplyReadouts(this.document.querySelectorAll<HTMLElement>('.supply-span'), supplies);
                this.document.querySelector('.board-details summary').removeAttribute('title');
            },
            error => {
                this.document.querySelector('.board-details summary').setAttribute('title', 'Board readback unavailable; retrying…');
                console.error('Alpha15 board readback failed:', error);
            });
        this.telemetry.start();
    }

    dispose(): void {
        this.disposed = true;
        this.events.dispose();
        this.telemetry?.dispose();
        this.precision?.dispose();
        this.clockControls?.dispose();
    }
}
