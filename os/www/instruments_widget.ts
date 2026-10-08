class InstrumentsWidget {
    private driver = new Instruments();
    private table: HTMLTableElement;
    private uploadInput: HTMLInputElement;
    private message: HTMLElement;
    private connection: HTMLElement;
    private busy = false;
    private reading = false;
    private signature = '';
    private poller: InstrumentPoller<any>;

    constructor(private document: Document) {
        this.table = document.getElementById('instruments-table') as HTMLTableElement;
        this.uploadInput = document.getElementById('upload-input') as HTMLInputElement;
        this.message = document.getElementById('upload-status');
        this.connection = document.getElementById('instruments-status');
        document.getElementById('upload-btn').addEventListener('click', () => this.uploadInput.click());
        this.uploadInput.addEventListener('change', () => this.uploadInstrumentClick());
        document.getElementById('refresh-instruments').addEventListener('click', () => this.refresh());
        this.poller = new InstrumentPoller(document, () => this.read(), status => {
            if (status) { this.render(status); }
        }, error => this.setConnection(String(error), true));
        this.poller.start();
        window.addEventListener('pagehide', event => { if (!event.persisted) { this.poller.dispose(); } });
    }

    private read(): Promise<any> {
        if (this.reading || this.busy) { return Promise.resolve(null); }
        this.reading = true;
        return new Promise((resolve, reject) => this.driver.getInstrumentsStatus(status => {
            this.reading = false;
            resolve(status);
        }, error => { this.reading = false; reject(error); }));
    }

    private refresh(): void {
        this.read().then(status => { if (status) { this.render(status); } },
            error => this.setConnection(String(error), true));
    }

    private setConnection(text: string, failed = false): void {
        this.connection.textContent = text;
        this.connection.hidden = !failed;
        const indicator = this.document.getElementById('board-connection');
        indicator.textContent = failed ? 'Disconnected' : 'Connected';
        indicator.dataset.state = failed ? 'error' : 'live';
        this.connection.dataset.state = failed ? 'error' : 'ready';
        if (failed) {
            this.document.getElementById('live-label').textContent = 'Instrument';
            this.document.getElementById('live-name').textContent = 'Status unavailable';
            this.document.getElementById('live-version').textContent = '';
            this.document.getElementById('open-instrument').hidden = true;
            this.signature = '';
        }
    }

    private render(status: any): void {
        if (this.busy) { return; }
        this.setConnection('');
        const signature = JSON.stringify(status);
        if (signature === this.signature) { return; }
        this.signature = signature;
        const live = status.live_instrument;
        this.document.getElementById('live-label').textContent = live ? 'Running' : 'Instrument';
        this.document.getElementById('live-name').textContent = live ? live.name : 'No instrument running';
        this.document.getElementById('live-version').textContent = live && live.version ? 'v' + live.version : '';
        this.document.getElementById('open-instrument').hidden = !live;
        this.document.getElementById('instrument-count').textContent = `(${status.instruments.length})`;
        const body = this.table.tBodies[0];
        body.textContent = '';
        const instruments = status.instruments.slice().sort((a: any, b: any) => {
            const rank = (item: any) => live && item.name === live.name ? 0 : item.is_default ? 1 : 2;
            return rank(a) - rank(b) || a.name.localeCompare(b.name);
        });
        for (const instrument of instruments) {
            const running = !!live && live.name === instrument.name;
            const row = body.insertRow();
            row.dataset.name = instrument.name;
            row.dataset.running = String(running);
            const name = this.document.createElement('a');
            name.href = '/koheron/instrument_summary.html?name=' + encodeURIComponent(instrument.name);
            name.textContent = instrument.name;
            row.insertCell().appendChild(name);
            row.insertCell().textContent = instrument.version || '—';
            const state = row.insertCell();
            if (running) { this.badge(state, 'Running', 'live'); }
            if (instrument.is_default) { this.badge(state, 'Default'); }
            if (!running && !instrument.is_default) { state.textContent = 'Ready'; state.className = 'state-ready'; }
            const actions = this.document.createElement('div');
            actions.className = 'row-actions';
            row.insertCell().appendChild(actions);
            if (running) {
                const open = this.document.createElement('a');
                open.href = '/'; open.textContent = 'Open ↗'; actions.appendChild(open);
            } else {
                this.button(actions, 'Run', () => {
                    this.begin(`Starting ${instrument.name}…`);
                    this.driver.runInstrument(instrument.name, (failed, error) => {
                        this.finish(failed ? error : `${instrument.name} is running.`, failed);
                    });
                });
            }
            if (!running && !instrument.is_default) {
                this.button(actions, 'Remove', () => {
                    if (!window.confirm(`Remove “${instrument.name}” from this board?`)) { return; }
                    this.begin(`Removing ${instrument.name}…`);
                    this.driver.deleteInstrument(instrument.name, (success, error) =>
                        this.finish(success ? `${instrument.name} removed.` : error, !success));
                }, 'remove');
            } else {
                const spacer = this.document.createElement('span');
                spacer.className = 'action-spacer'; actions.appendChild(spacer);
            }
        }
        if (!status.instruments.length) {
            const cell = body.insertRow().insertCell();
            cell.colSpan = 4; cell.textContent = 'No instruments installed. Add an instrument ZIP to get started.';
        }
    }

    private badge(cell: HTMLElement, text: string, kind = ''): void {
        const badge = this.document.createElement('span');
        badge.className = 'badge ' + kind; badge.textContent = text; cell.appendChild(badge);
    }

    private button(parent: HTMLElement, text: string, action: () => void, kind = ''): void {
        const button = this.document.createElement('button');
        button.type = 'button'; button.textContent = text; button.className = kind;
        button.setAttribute('aria-label', text + ' ' + parent.closest('tr').dataset.name);
        button.addEventListener('click', action); parent.appendChild(button);
    }

    private feedback(text: string, failed = false): void {
        this.message.hidden = false;
        this.message.textContent = text;
        this.message.dataset.state = failed ? 'error' : 'ready';
    }

    private begin(text: string): void {
        this.busy = true;
        this.feedback(text);
        this.setDisabled(true);
    }

    private setDisabled(disabled: boolean): void {
        this.document.querySelectorAll<HTMLButtonElement>('#instruments-table button, #upload-btn, #refresh-instruments')
            .forEach(button => button.disabled = disabled);
    }

    private finish(text: string, failed: boolean): void {
        this.busy = false;
        this.setDisabled(false);
        this.feedback(text, failed);
        this.refresh();
    }

    uploadInstrumentClick(): void {
        const file = this.uploadInput.files && this.uploadInput.files[0];
        if (!file || this.busy) { return; }
        this.uploadInput.value = '';
        this.upload(file);
    }

    private upload(file: File): void {
        if (!/\.zip$/i.test(file.name)) { this.feedback('Select an instrument ZIP file.', true); return; }
        this.begin(`Uploading ${file.name}…`);
        this.driver.uploadInstrument(file, (success, error) =>
            this.finish(success ? `${file.name} uploaded. Select Run to start it.` : error, !success));
    }
}
