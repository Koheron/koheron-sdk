class PnaSampleRate {
    private select: HTMLSelectElement;
    private status: HTMLElement;
    private disposed = false;
    private busy = false;
    private revision = 0;
    private rate = 0;
    private timer: number;
    private change = () => { void this.apply(); };

    constructor(private document: Document, private driver: PhaseNoiseAnalyzer,
                private onRate: (rate: number) => void,
                private onError: (error: unknown) => void) {
        this.select = document.querySelector('#sample-rate');
        this.status = document.querySelector('#sample-rate-status');
        this.select.addEventListener('change', this.change);
    }

    async init(): Promise<void> {
        if (!this.driver.supportsSampleRate()) {
            const parameters = await this.driver.getParameters();
            this.render(Math.round(parameters.fs * 2 * parameters.cic_rate));
            this.select.disabled = true;
            this.select.title = 'Update the instrument to enable sample-rate switching.';
            return;
        }
        this.render(await this.driver.getSamplingFrequency());
        if (this.disposed) { return; }
        this.select.disabled = false;
        this.timer = window.setTimeout(() => { void this.poll(); }, 1000);
    }

    dispose(): void {
        this.disposed = true;
        window.clearTimeout(this.timer);
        this.select.disabled = true;
        this.select.removeEventListener('change', this.change);
    }

    private render(rate: number): void {
        if (this.disposed) { return; }
        this.select.value = String(rate);
        this.document.getElementById('sample-rate-label').textContent = String(rate / 1e6);
        if (rate !== this.rate) { this.rate = rate; this.onRate(rate); }
    }

    private async apply(): Promise<void> {
        if (this.disposed || this.busy || this.select.disabled) { return; }
        this.busy = true;
        ++this.revision;
        this.select.disabled = true;
        this.status.textContent = 'Switching…';
        try {
            const accepted = await this.driver.setSamplingFrequency(Number(this.select.value));
            this.render(await this.driver.getSamplingFrequency());
            if (!this.disposed) this.status.textContent = accepted ? '' :
                'Rate change rejected. Check LO/DAC frequencies and the sample clock.';
        } catch (error) {
            if (!this.disposed) { this.onError(error); }
        } finally {
            this.busy = false;
            if (!this.disposed) { this.select.disabled = false; }
        }
    }

    private async poll(): Promise<void> {
        if (this.disposed) { return; }
        const revision = this.revision;
        try {
            if (!this.busy) {
                const rate = await this.driver.getSamplingFrequency();
                if (!this.busy && revision === this.revision) { this.render(rate); }
            }
        } catch (error) {
            if (!this.disposed) { this.onError(error); }
        } finally {
            if (!this.disposed) { this.timer = window.setTimeout(() => { void this.poll(); }, 1000); }
        }
    }
}
