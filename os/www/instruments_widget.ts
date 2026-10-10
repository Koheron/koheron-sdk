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
    private localBusy = false;
    private lastOperation = -1;
    private snapshot: RuntimeStatus;
    private inspection = 0;
    private unavailable = false;
    private pendingMessage = '';
    private pendingRevision = -1;
    private inspectionSource: HTMLElement;

    constructor(private document: Document, private runtime?: RuntimeStream) {
        this.table = document.getElementById('instruments-table') as HTMLTableElement;
        this.uploadInput = document.getElementById('upload-input') as HTMLInputElement;
        this.message = document.getElementById('upload-status');
        this.connection = document.getElementById('instruments-status');
        document.getElementById('upload-btn').addEventListener('click', () => this.uploadInput.click());
        this.uploadInput.addEventListener('change', () => this.uploadInstrumentClick());
        document.getElementById('refresh-instruments').addEventListener('click', () => this.refresh());
        if (runtime) {
            this.setConnection('Connecting to the board…', true, true);
            runtime.subscribe(status => this.renderRuntime(status), error => this.setConnection(error, true));
            for (const action of ['start', 'stop', 'restart'] as const) {
                document.getElementById('instrument-' + action).addEventListener('click', () =>
                    this.perform(`${action === 'stop' ? 'Stopping' : action === 'restart' ? 'Restarting' : 'Starting'} instrument…`, () => this.driver.control(action), true));
            }
            document.getElementById('preflight-close').addEventListener('click', () => {
                ++this.inspection; document.getElementById('preflight-panel').hidden = true;
                if (this.inspectionSource?.isConnected) { this.inspectionSource.focus(); }
            });
            document.addEventListener('click', event => {
                const selected = (event.target as Element).closest('.instrument-options');
                this.table.querySelectorAll<HTMLDetailsElement>('.instrument-options[open]').forEach(menu => {
                    if (menu !== selected) { menu.open = false; }
                });
            });
            document.addEventListener('keydown', event => {
                if (event.key !== 'Escape') { return; }
                const menu = this.table.querySelector<HTMLDetailsElement>('.instrument-options[open]');
                if (menu) { menu.open = false; menu.querySelector('summary').focus(); event.preventDefault(); }
            });
        } else {
            this.poller = new InstrumentPoller(document, () => this.read(), status => {
                if (status) { this.render(status); }
            }, error => this.setConnection(String(error), true));
            this.poller.start();
            window.addEventListener('pagehide', event => { if (!event.persisted) { this.poller.dispose(); } });
        }
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
        if (this.runtime) { void this.runtime.refresh(); return; }
        this.read().then(status => { if (status) { this.render(status); } },
            error => this.setConnection(String(error), true));
    }

    private setConnection(text: string, failed = false, connecting = false): void {
        this.unavailable = failed;
        this.connection.textContent = text;
        this.connection.hidden = !failed;
        const indicator = this.document.getElementById('board-connection');
        indicator.textContent = connecting ? 'Connecting…' : failed ? 'Disconnected' : 'Connected';
        indicator.dataset.state = connecting ? 'connecting' : failed ? 'error' : 'live';
        this.connection.dataset.state = connecting ? 'loading' : failed ? 'error' : 'ready';
        this.document.getElementById('refresh-instruments').textContent = failed && !connecting ? 'Retry' : 'Refresh';
        if (failed) {
            this.document.getElementById('live-label').textContent = 'Instrument';
            this.document.getElementById('live-name').textContent = connecting ? 'Connecting…' : 'Status unavailable';
            this.document.getElementById('live-version').textContent = '';
            this.document.getElementById('open-instrument').hidden = true;
            this.signature = '';
            if (this.runtime) {
                this.document.getElementById('runtime-operation').hidden = true;
                this.setDisabled(true);
            }
        }
    }

    private renderRuntime(status: RuntimeStatus): void {
        this.snapshot = status;
        this.busy = this.localBusy || status.operation.busy;
        this.render(status.instruments);
        const current = status.current_instrument;
        const running = status.instruments.live_instrument;
        const operation = status.operation;
        if (operation.busy) {
            this.document.getElementById('live-label').textContent = 'Instrument';
            this.document.getElementById('live-name').textContent = operation.instrument || current?.name || 'Changing instrument…';
            this.document.getElementById('open-instrument').hidden = true;
        } else if (current && !running) {
            this.document.getElementById('live-label').textContent = 'Stopped';
            this.document.getElementById('live-name').textContent = current.name;
            this.document.getElementById('live-version').textContent = current.version ? 'v' + current.version : '';
        }
        for (const action of ['start', 'stop', 'restart']) {
            const button = this.document.getElementById('instrument-' + action) as HTMLButtonElement;
            button.hidden = !current || (action === 'start' ? !!running : !running);
            button.disabled = this.busy;
        }
        const progress = this.document.getElementById('runtime-operation');
        const phases: Record<string, string> = {validating: 'Checking compatibility and free space…', extracting: 'Preparing instrument files…',
            stopping: 'Stopping instrument…', starting: 'Starting instrument…', restarting: 'Restarting instrument…', rolling_back: 'Restoring previous instrument…'};
        const pending = !!this.pendingMessage && operation.revision <= this.pendingRevision;
        progress.hidden = !pending && operation.phase === 'idle';
        progress.dataset.state = pending || operation.busy ? 'loading' : operation.phase === 'failed' ? 'error' : 'ready';
        const outcomes: Record<string, string> = {start: 'Instrument started.', stop: 'Instrument stopped.', restart: 'Instrument restarted.',
            activate: `${operation.instrument || current?.name || 'Instrument'} is running.`};
        progress.textContent = pending ? this.pendingMessage : operation.busy ? phases[operation.phase] || 'Changing instrument…' :
            operation.phase === 'succeeded' ? outcomes[operation.action] || operation.message : operation.message;
        if (!pending && !operation.busy && operation.rollback === 'restored') { progress.textContent += ' Previous instrument restored.'; }
        if (!pending && !operation.busy && operation.rollback === 'failed') { progress.textContent += ' Restoration failed; see the instrument log.'; }
        if (operation.revision !== this.lastOperation) {
            this.lastOperation = operation.revision;
            if (operation.busy) { ++this.inspection; this.document.getElementById('preflight-panel').hidden = true; }
        }
        this.setDisabled(this.busy);
    }

    private async perform(message: string, action: () => Promise<any>, operation = false): Promise<void> {
        if (this.busy || this.unavailable) { return; }
        if (operation) { this.pendingMessage = message; this.pendingRevision = this.snapshot?.operation.revision ?? -1; }
        this.begin(message);
        if (operation) {
            this.message.hidden = true;
            const status = this.document.getElementById('runtime-operation');
            status.hidden = false; status.textContent = message; status.dataset.state = 'loading';
        }
        try {
            const result = await action();
            this.finish(result.message || (result.default_instrument ? `${result.default_instrument} will start at boot.` : 'Completed.'), false, !operation);
        } catch (error) { this.finish(String(error).replace(/^Error: /, ''), true); }
    }

    private async inspect(name: string, source: HTMLElement): Promise<void> {
        const inspection = ++this.inspection;
        this.inspectionSource = source;
        const panel = this.document.getElementById('preflight-panel');
        panel.hidden = false;
        this.document.getElementById('preflight-name').textContent = name;
        const content = this.document.getElementById('preflight-content'); content.textContent = 'Checking instrument…';
        this.document.getElementById('preflight-close').focus();
        try {
            const result = await this.driver.preflight(name);
            if (inspection === this.inspection) new PreflightView(this.document, content).render(result);
        }
        catch (error) { if (inspection === this.inspection) content.textContent = String(error).replace(/^Error: /, ''); }
    }

    private render(status: any): void {
        if (this.busy && !this.runtime) { return; }
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
            const version = this.document.createElement('span'); version.className = 'instrument-version';
            version.textContent = instrument.version || '—'; version.title = instrument.version || '';
            row.insertCell().appendChild(version);
            const state = row.insertCell();
            if (running) { this.badge(state, 'Running', 'live'); }
            if (instrument.is_default) { this.badge(state, 'Default'); }
            if (!running && !instrument.is_default) { state.textContent = this.runtime ? 'Installed' : 'Ready'; state.className = 'state-ready'; }
            const actions = this.document.createElement('div');
            actions.className = 'row-actions';
            row.insertCell().appendChild(actions);
            if (running) {
                const open = this.document.createElement('a');
                open.href = '/'; open.textContent = 'Open ↗'; actions.appendChild(open);
            } else {
                this.button(actions, 'Run', () => {
                    if (this.runtime) { void this.perform(`Starting ${instrument.name}…`, () => this.driver.activate(instrument.name), true); return; }
                    this.begin(`Starting ${instrument.name}…`);
                    this.driver.runInstrument(instrument.name, (failed, error) => {
                        this.finish(failed ? error : `${instrument.name} is running.`, failed);
                    });
                });
            }
            let extraActions: HTMLElement = actions;
            if (this.runtime) {
                const options = this.document.createElement('details'); options.className = 'instrument-options';
                const summary = this.document.createElement('summary'); summary.textContent = 'More';
                summary.setAttribute('aria-label', 'More actions for ' + instrument.name); options.appendChild(summary);
                actions.appendChild(options);
                const menu = this.document.createElement('div'); menu.className = 'instrument-menu'; options.appendChild(menu);
                extraActions = menu;
                this.button(menu, 'Check instrument', () => { options.open = false; void this.inspect(instrument.name, summary); });
                if (!instrument.is_default) {
                    this.button(menu, 'Start at boot', () => {
                        options.open = false; void this.perform(`Selecting ${instrument.name} for boot…`, () => this.driver.setDefault(instrument.name));
                    });
                }
            }
            if (!running && !instrument.is_default) {
                this.button(extraActions, 'Remove', () => {
                    if (!window.confirm(`Remove “${instrument.name}” from this board?`)) { return; }
                    extraActions.closest('details')?.removeAttribute('open');
                    this.begin(`Removing ${instrument.name}…`);
                    this.driver.deleteInstrument(instrument.name, (success, error) =>
                        this.finish(success ? `${instrument.name} removed.` : error, !success));
                }, 'remove');
            } else if (!this.runtime) {
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
        this.localBusy = true;
        this.feedback(text);
        this.setDisabled(true);
    }

    private setDisabled(disabled: boolean): void {
        this.document.querySelectorAll<HTMLButtonElement>('#instruments-table button, #upload-btn, #instrument-start, #instrument-stop, #instrument-restart')
            .forEach(button => button.disabled = disabled || (!!this.runtime && this.unavailable));
    }

    private finish(text: string, failed = false, showFeedback = true): void {
        if (failed && this.pendingMessage) { this.document.getElementById('runtime-operation').hidden = true; }
        this.pendingMessage = '';
        this.localBusy = false;
        this.busy = !!this.runtime && !!this.snapshot?.operation.busy;
        this.setDisabled(this.busy);
        if (showFeedback) this.feedback(text, failed);
        else this.message.hidden = true;
        this.refresh();
    }

    uploadInstrumentClick(): void {
        const file = this.uploadInput.files && this.uploadInput.files[0];
        if (!file || this.busy || this.unavailable) { return; }
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
