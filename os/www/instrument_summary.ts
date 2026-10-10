class InstrumentSummaryWidget {
    private instrumentsDriver: Instruments;
    private instrumentName: string | null;
    private nameElement: HTMLElement | null;
    private versionElement: HTMLElement | null;
    private commandsContainer: HTMLElement | null;
    private commandsStatus: HTMLElement | null;
    private checking = false;
    private loadingCommands = false;

    constructor(private document: Document) {
        this.instrumentsDriver = new Instruments();
        this.instrumentName = this.getInstrumentName();
        this.nameElement = document.getElementById('instrument-name');
        this.versionElement = document.getElementById('instrument-version');
        this.commandsContainer = document.getElementById('instrument-commands');
        this.commandsStatus = document.getElementById('instrument-commands-status');

        if (!this.instrumentName) {
            this.nameElement.textContent = 'No instrument selected';
            this.setDetailsStatus('Choose an instrument from the installed instruments list.');
            document.getElementById('instrument-check-heading').closest('section').hidden = true;
            document.getElementById('commands-heading').closest('section').hidden = true;
            return;
        }

        if (this.nameElement) {
            this.nameElement.textContent = this.instrumentName;
        }

        this.loadInstrumentDetails();
        this.loadCommands();
        document.getElementById('instrument-commands-retry').addEventListener('click', () => this.loadCommands());
        document.getElementById('instrument-check-refresh').addEventListener('click', () => this.loadPreflight());
        void this.loadPreflight();
    }

    private async loadPreflight(): Promise<void> {
        if (this.checking) { return; }
        this.checking = true;
        const container = this.document.getElementById('instrument-check');
        const button = this.document.getElementById('instrument-check-refresh') as HTMLButtonElement;
        button.disabled = true; button.textContent = 'Checking…';
        container.textContent = 'Checking instrument…';
        try { new PreflightView(this.document, container).render(await this.instrumentsDriver.preflight(this.instrumentName)); }
        catch (error) { container.textContent = String(error).replace(/^Error: /, ''); }
        finally { this.checking = false; button.disabled = false; button.textContent = 'Check again'; }
    }

    private getInstrumentName(): string | null {
        const params = new URLSearchParams(window.location.search);
        const name = params.get('name');
        return name || null;
    }

    private loadInstrumentDetails(): void {
        this.instrumentsDriver.getInstrumentsStatus((status) => {
            const instruments = status['instruments'] as any[] | undefined;
            if (!Array.isArray(instruments)) {
                this.setDetailsStatus('Cannot load instrument details.');
                return;
            }

            const instrument = instruments.find((inst) => inst['name'] === this.instrumentName);
            if (!instrument) {
                this.setDetailsStatus('This instrument is no longer installed. Return to the installed instruments list.');
                return;
            }

            if (this.versionElement) {
                const version = instrument['version'] || 'Unknown';
                this.versionElement.textContent = version;
            }
        }, error => this.setDetailsStatus(error));
    }

    private loadCommands(): void {
        if (!this.instrumentName || this.loadingCommands) {
            return;
        }
        this.loadingCommands = true;
        this.setStatus('Loading commands…');
        const finish = (message: string, failed = false) => { this.loadingCommands = false; this.setStatus(message, failed); };
        const xhr = new XMLHttpRequest();
        xhr.open('GET', '/api/instruments/commands/' + encodeURIComponent(this.instrumentName), true);
        xhr.timeout = 8000;
        xhr.onload = () => {
            if (xhr.readyState !== 4) {
                return;
            }

            if (xhr.status === 200) {
                try {
                    const data = JSON.parse(xhr.responseText);
                    if (!Array.isArray(data)) { throw new Error('Invalid commands'); }
                    this.renderCommands(data);
                    this.loadingCommands = false;
                } catch (err) {
                    this.commandsContainer.textContent = '';
                    finish('Cannot read the command description. Try again.', true);
                }
            } else if (xhr.status === 404) {
                finish('No command description available.');
            } else {
                finish('Cannot load commands (HTTP ' + xhr.status + ').', true);
            }
        };
        xhr.onerror = () => finish('Cannot reach the board. Retry when the connection is restored.', true);
        xhr.ontimeout = () => finish('Loading commands timed out. Try again.', true);
        xhr.send(null);
    }

    private renderCommands(data: any): void {
        if (!this.commandsContainer) {
            return;
        }

        this.commandsContainer.textContent = '';
        this.setStatus('');

        if (!Array.isArray(data) || data.length === 0) {
            this.setStatus('No commands defined for this instrument.');
            return;
        }

        for (const driver of data) {
            const className = driver['class'] || 'Driver';
            if (className === 'KServer') {
                continue;
            }

            const details = document.createElement('details');
            details.className = 'command-group';

            const summary = document.createElement('summary');
            summary.className = 'command-group-title';
            summary.textContent = className;
            details.appendChild(summary);

            const list = document.createElement('ul');
            list.className = 'command-list';

            if (Array.isArray(driver['functions']) && driver['functions'].length > 0) {
                for (const func of driver['functions']) {
                    list.appendChild(this.createCommandListItem(func));
                }
            } else {
                const li = document.createElement('li');
                li.textContent = 'No commands exposed.';
                list.appendChild(li);
            }

            details.appendChild(list);
            this.commandsContainer.appendChild(details);
        }
        if (!this.commandsContainer.childElementCount) { this.setStatus('No commands exposed by this instrument.'); }
    }

    private createCommandListItem(func: any): HTMLLIElement {
        const li = document.createElement('li');
        li.className = 'command-entry';

        const name = func && func['name'] ? func['name'] : 'command';
        const args = Array.isArray(func && func['args']) ? func['args'] : [];
        const retType = this.cleanType(func && func['ret_type'] ? func['ret_type'] : 'void');

        const nameSpan = document.createElement('span');
        nameSpan.className = 'command-name';
        nameSpan.textContent = name;
        li.appendChild(nameSpan);
        li.appendChild(document.createTextNode(' ('));

        args.forEach((arg: any, index: number) => {
            if (index > 0) {
                li.appendChild(document.createTextNode(', '));
            }

            const argType = this.cleanType(arg && arg['type'] ? arg['type'] : 'unknown');
            const argName = arg && arg['name'] ? arg['name'] : 'arg' + index;

            const typeSpan = document.createElement('span');
            typeSpan.className = 'command-input-type';
            typeSpan.textContent = argType;
            li.appendChild(typeSpan);
            li.appendChild(document.createTextNode(' '));

            const nameSpanArg = document.createElement('span');
            nameSpanArg.className = 'command-arg-name';
            nameSpanArg.textContent = argName;
            li.appendChild(nameSpanArg);
        });

        li.appendChild(document.createTextNode(') → '));

        const retSpan = document.createElement('span');
        retSpan.className = 'command-output-type';
        retSpan.textContent = retType;
        li.appendChild(retSpan);

        return li;
    }

    private cleanType(type: string): string {
        if (!type) {
            return '';
        }

        let cleaned = type;

        // Remove std:: namespace qualifiers.
        cleaned = cleaned.replace(/std::/g, '');

        // Simplify allocator-qualified vector types.
        cleaned = cleaned.replace(/vector\s*<\s*([^,>]+?)\s*,\s*allocator\s*<[^>]+>\s*>/g, 'vector<$1>');

        // Drop unsigned/long suffixes on numeric literals.
        cleaned = cleaned.replace(/\b(\d+)(?:[uUlL]+)\b/g, '$1');

        return cleaned.trim();
    }

    private setDetailsStatus(message: string): void {
        const status = this.document.getElementById('instrument-details-status');
        status.textContent = message; status.hidden = !message; status.dataset.state = 'error';
    }

    private setStatus(message: string, failed = false): void {
        if (this.commandsStatus) {
            this.commandsStatus.textContent = message;
            this.commandsStatus.hidden = !message;
            this.commandsStatus.dataset.state = failed ? 'error' : 'ready';
        }
        this.document.getElementById('instrument-commands-retry').hidden = !failed;
    }
}
