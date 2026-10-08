// Slow board readbacks share a lifecycle; hosts supply transport and rendering.
class InstrumentPoller<T> {
    private stopped = false;
    private busy = false;
    private started = false;
    private timer: number;

    constructor(private document: Document, private read: () => Promise<T>,
                private render: (value: T) => void,
                private onError: (error: unknown) => void = error => console.error('Board readback failed:', error)) {}

    start(): void {
        if (this.started || this.stopped) { return; }
        this.started = true;
        void this.poll();
    }

    private async poll(): Promise<void> {
        if (this.stopped || this.busy) { return; }
        this.document.defaultView.clearTimeout(this.timer);
        this.busy = true;
        try {
            if (!this.document.hidden) {
                const value = await this.read();
                if (!this.stopped) { this.render(value); }
            }
        } catch (error) {
            if (!this.stopped) { this.onError(error); }
        } finally {
            this.busy = false;
            if (!this.stopped) {
                this.timer = this.document.defaultView.setTimeout(() => this.poll(), 1000);
            }
        }
    }

    dispose(): void {
        this.stopped = true;
        this.document.defaultView.clearTimeout(this.timer);
    }
}
