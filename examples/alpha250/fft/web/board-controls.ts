class Alpha250FFTControls implements FFTBoardControls {
    private disposed = false;
    private precision: PrecisionChannelsApp;
    private clock: ClockGeneratorApp;

    constructor(private client: Client, private document: Document) {}

    async init(): Promise<void> {
        this.clock = new ClockGeneratorApp(this.document, new ClockGenerator(this.client));
        this.precision = new PrecisionChannelsApp(this.document, new PrecisionDac(this.client));
        await this.precision.init();
        if (this.disposed) { return; }
        this.document.getElementById('sampling-frequency').hidden = false;
        this.document.getElementById('reference-clock').hidden = false;
        this.document.querySelector<HTMLDetailsElement>('.board-details').hidden = false;
        (this.document.getElementById('board-controls') as HTMLFieldSetElement).disabled = false;
    }

    precisionDacChanged(values: ArrayLike<number>): void { this.precision?.setValues(values); }

    dispose(): void {
        this.disposed = true;
        this.precision?.dispose();
    }
}
