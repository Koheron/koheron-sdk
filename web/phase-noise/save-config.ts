// Shared feedback for checked saves, with request-only feedback for older servers.
class PnaSaveConfig {
    private button: HTMLButtonElement;
    private status: HTMLElement;
    private timer: number;
    private disposed = false;
    private pending = false;
    private events = new InstrumentEvents();
    private onClick = async (): Promise<void> => {
        if (this.disposed || this.pending) { return; }
        this.document.defaultView.clearTimeout(this.timer);
        this.pending = true;
        this.button.disabled = true;
        this.button.textContent = 'Saving…';
        if (this.status) { this.status.textContent = 'Saving analyzer settings…'; }
        try {
            const saved = await this.save();
            if (this.disposed) { return; }
            this.button.textContent = saved === true ? 'Saved' : saved === false ? 'Save failed' : 'Save requested';
            if (this.status) {
                this.status.textContent = saved === true ? 'Analyzer settings saved.' : saved === false ?
                    'Analyzer settings could not be saved. Check instrument logs.' : 'Analyzer settings save requested.';
            }
        } catch (error) {
            if (this.disposed) { return; }
            this.button.textContent = 'Save failed';
            if (this.status) { this.status.textContent = 'Unable to confirm the save request.'; }
            this.onError(error);
        } finally {
            this.pending = false;
            if (!this.disposed) { this.button.disabled = false; }
        }
        if (this.disposed) { return; }
        this.timer = this.document.defaultView.setTimeout(() => {
            if (!this.disposed) { this.button.textContent = 'Save settings'; }
        }, 2000);
    };

    constructor(private document: Document, private save: () => boolean | void | Promise<boolean | void>,
                private onError: (error: unknown) => void = () => {}) {
        this.button = document.querySelector<HTMLButtonElement>('.save-cfg');
        this.status = document.getElementById('save-config-status');
        if (this.button) { this.events.listen(this.button, 'click', this.onClick); }
    }

    dispose(): void {
        if (this.disposed) { return; }
        this.disposed = true;
        this.events.dispose();
        this.document.defaultView.clearTimeout(this.timer);
        if (this.pending && this.button) { this.button.disabled = false; }
    }
}
