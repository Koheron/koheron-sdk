class ClockGeneratorApp {
    private events = new InstrumentEvents();
    private onChange = (event: Event): void => {
        const input = event.currentTarget as HTMLInputElement;
        if (this.settingsChanged) { this.settingsChanged(); }
        this.driver[input.dataset.command](parseInt(input.value));
    };

    constructor(document: Document, private driver, private settingsChanged?: () => void) {
        const inputs = Array.from(document.getElementsByClassName('clkgen-input')) as HTMLInputElement[];
        for (const input of inputs) { this.events.listen(input, 'change', this.onChange); }
    }

    dispose(): void { this.events.dispose(); }
}
