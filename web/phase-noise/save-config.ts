// Shared request feedback for the PNA save RPC, which has no acknowledgement.
class PnaSaveConfig {
    private button: HTMLButtonElement;
    private status: HTMLElement;
    private timer: number;
    private disposed = false;
    private events = new InstrumentEvents();
    private onClick = (): void => {
        if (this.disposed) { return; }
        this.document.defaultView.clearTimeout(this.timer);
        try {
            this.save();
            this.button.textContent = 'Save requested';
            if (this.status) { this.status.textContent = 'Analyzer settings save requested.'; }
        } catch (error) {
            this.button.textContent = 'Save failed';
            if (this.status) { this.status.textContent = 'Unable to send the save request.'; }
            this.onError(error);
        }
        if (this.disposed) { return; }
        this.timer = this.document.defaultView.setTimeout(() => {
            if (!this.disposed) { this.button.textContent = 'Save settings'; }
        }, 2000);
    };

    constructor(private document: Document, private save: () => void,
                private onError: (error: unknown) => void = () => {}) {
        this.button = document.querySelector<HTMLButtonElement>('.save-cfg');
        this.status = document.getElementById('save-config-status');
        if (this.button) { this.events.listen(this.button, 'click', this.onClick); }
    }

    dispose(): void {
        this.disposed = true;
        this.events.dispose();
        this.document.defaultView.clearTimeout(this.timer);
    }
}
