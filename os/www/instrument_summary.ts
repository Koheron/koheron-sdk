class InstrumentSummaryWidget {
    private instrumentsDriver: Instruments;
    private instrumentName: string | null;
    private nameElement: HTMLElement | null;
    private versionElement: HTMLElement | null;
    private commandsContainer: HTMLElement | null;
    private commandsStatus: HTMLElement | null;

    constructor(document: Document) {
        this.instrumentsDriver = new Instruments();
        this.instrumentName = this.getInstrumentName();
        this.nameElement = document.getElementById('instrument-name');
        this.versionElement = document.getElementById('instrument-version');
        this.commandsContainer = document.getElementById('instrument-commands');
        this.commandsStatus = document.getElementById('instrument-commands-status');

        if (!this.instrumentName) {
            this.setStatus('Instrument name missing in URL.');
            return;
        }

        if (this.nameElement) {
            this.nameElement.textContent = this.instrumentName;
        }

        this.loadInstrumentDetails();
        this.loadCommands();
    }

    private getInstrumentName(): string | null {
        const params = new URLSearchParams(window.location.search);
        const name = params.get('name');
        return name ? decodeURIComponent(name) : null;
    }

    private loadInstrumentDetails(): void {
        this.instrumentsDriver.getInstrumentsStatus((status) => {
            const instruments = status['instruments'] as any[] | undefined;
            if (!Array.isArray(instruments)) {
                this.setStatus('Cannot load instrument details.');
                return;
            }

            const instrument = instruments.find((inst) => inst['name'] === this.instrumentName);
            if (!instrument) {
                this.setStatus('Instrument not found.');
                return;
            }

            if (this.versionElement) {
                const version = instrument['version'] || 'Unknown';
                this.versionElement.textContent = version;
            }
        });
    }

    private loadCommands(): void {
        if (!this.instrumentName) {
            return;
        }

        const xhr = new XMLHttpRequest();
        xhr.open('GET', '/api/instruments/commands/' + encodeURIComponent(this.instrumentName), true);
        xhr.onload = () => {
            if (xhr.readyState !== 4) {
                return;
            }

            if (xhr.status === 200) {
                try {
                    const data = JSON.parse(xhr.responseText);
                    this.renderCommands(data);
                } catch (err) {
                    this.setStatus('Cannot parse commands definition.');
                }
            } else if (xhr.status === 404) {
                this.setStatus('No command description available.');
            } else {
                this.setStatus('Cannot load commands (HTTP ' + xhr.status + ').');
            }
        };
        xhr.onerror = () => {
            this.setStatus('Failed to load commands.');
        };
        xhr.send(null);
    }

    private renderCommands(data: any): void {
        if (!this.commandsContainer) {
            return;
        }

        this.commandsContainer.innerHTML = '';
        if (this.commandsStatus) {
            this.commandsStatus.textContent = '';
        }

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

    private setStatus(message: string): void {
        if (this.commandsStatus) {
            this.commandsStatus.textContent = message;
        }
    }
}
