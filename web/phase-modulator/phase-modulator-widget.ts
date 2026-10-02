// Mount into any element; selectors, styles and listeners stay inside that root.
// No socket ownership, page globals, polling or writes during initialization.
class PhaseModulatorWidget {
    private info: PhaseModulatorInfo[] = [];
    private values: PhaseModulatorSettings[] = [];
    private busy: boolean[] = [];
    private disposed = false;
    private frequencies: Array<{carrier: FrequencyInput; modulation: FrequencyInput}> = [];
    private operations: Array<Promise<void>> = [];
    private changeHandler = (event: Event) => this.change(event);
    private clickHandler = (event: Event) => this.click(event);
    private shapes = ['Sine', 'Square', 'Pulse', 'Triangle', 'Up ramp', 'Down ramp',
                      'Uniform noise', 'Gaussian noise', 'PRBS', 'BPSK'];

    constructor(private root: HTMLElement, private driver: PhaseModulatorPort) {
        root.classList.add('dds-pm-widget');
        root.innerHTML = `<div class="pm-loading" role="status">Reading signal generator…</div>`;
        root.addEventListener('change', this.changeHandler);
        root.addEventListener('click', this.clickHandler);
    }

    async init(): Promise<void> {
        try {
            const info = await this.driver.init();
            const values = await Promise.all(info.map((_, channel) => this.driver.settings(channel)));
            if (this.disposed) { return; }
            this.info = info;
            this.values = values;
            this.busy = info.map(() => false);
            this.operations = info.map(() => Promise.resolve());
            this.frequencies.forEach(controls => { controls.carrier.dispose(); controls.modulation.dispose(); });
            this.root.innerHTML = `<div class="pm-toolbar"><strong>Signal generator</strong>
                <span class="pm-clock">${info[0].sampleRate / 1e6} MS/s</span>
                <button type="button" data-action="refresh" title="Read settings from hardware">Refresh</button></div>
                <div class="pm-channels">${info.map((_, channel) => this.channelMarkup(channel)).join('')}</div>
                <p class="pm-footnote">Edits preserve oscillator phase · Output amplitude is full scale</p>`;
            this.frequencies = info.map((metadata, channel) => {
                const controls = {} as {carrier: FrequencyInput; modulation: FrequencyInput};
                for (const field of ['carrier', 'modulation']) {
                    const section = this.channel(channel);
                    controls[field] = new FrequencyInput(section.querySelector(`[data-field="${field}"]`),
                        section.querySelector(`[data-frequency-unit="${field}"]`), {
                            value: values[channel][field], maximum: metadata.sampleRate / 2,
                            resolution: metadata.sampleRate / Math.pow(2, metadata.phaseWidth),
                            commit: async hz => {
                                const result = await this.perform(channel, () => this.driver.set(channel, field as PhaseModulatorField, hz), 'Applying…', true);
                                if (result.error) { throw new Error(result.error); }
                                return result.settings[field];
                            }
                        });
                }
                return controls;
            });
            info.forEach((_, channel) => this.render(channel));
            this.root.dispatchEvent(new CustomEvent('dds-pm-ready', {bubbles: true}));
        } catch (error) {
            if (!this.disposed) {
                this.root.innerHTML = `<div class="pm-load-error" role="alert"><span></span>
                    <button type="button" data-action="retry">Retry</button></div>`;
                this.root.querySelector('span').textContent = this.message(error);
            }
            throw error;
        }
    }

    private input(field: string, title: string, unit: string, min: string, max: string, step: string = 'any'): string {
        if (field === 'carrier' || field === 'modulation') {
            return `<label class="pm-field pm-${field}"><span>${title}</span><span class="pm-frequency-control">
                <input type="text" data-field="${field}" aria-label="${title} frequency" inputmode="decimal"
                    autocomplete="off" spellcheck="false" class="pm-frequency-number">
                <select data-frequency-unit="${field}" aria-label="${title} unit">
                    ${['Hz', 'kHz', 'MHz', 'GHz'].map(name => `<option ${name === unit ? 'selected' : ''}>${name}</option>`).join('')}
                </select><span class="pm-frequency-help" role="status"></span></span></label>`;
        }
        return `<label class="pm-field pm-${field}"><span>${title}</span><span class="pm-input-unit">
            <input type="number" data-field="${field}" aria-label="${title}${unit ? ' in ' + unit : ''}"
                required min="${min}" ${max ? 'max="' + max + '"' : ''} step="${step}">
            ${unit ? '<span>' + unit + '</span>' : ''}</span></label>`;
    }

    private channelMarkup(channel: number): string {
        const info = this.info[channel];
        const options = this.shapes.map((name, code) => info.capabilities & (1 << code) ? `<option value="${code}">${name}</option>` : '').join('');
        const nyquist = info.sampleRate / 2;
        return `<section class="pm-channel" data-channel="${channel}" aria-label="DAC ${channel}">
            <fieldset><legend class="pm-sr-only">DAC ${channel} settings</legend><div class="pm-row">
                <div class="pm-output"><strong>DAC ${channel}</strong><button type="button" data-action="output"
                    aria-pressed="false" aria-label="Enable DAC ${channel} output">Muted</button></div>
                ${this.input('carrier', 'Carrier', 'MHz', '0', String(nyquist / 1e6))}
                <label class="pm-pm"><input type="checkbox" data-field="pm" ${!options ? 'disabled' : ''}>PM</label>
                <label class="pm-field pm-waveform"><span>Source</span><select data-field="waveform" aria-label="PM source">
                    ${options || '<option value="0">No PM sources</option>'}</select></label>
                ${this.input('modulation', 'Rate', 'kHz', '0', String(nyquist / 1e3))}
                ${this.input('deviation', 'Amplitude', '°', '0', '360')}
                <button type="button" class="pm-more" data-action="more" aria-expanded="false" aria-label="More DAC ${channel} settings">More <span aria-hidden="true">▾</span></button>
            </div><div class="pm-advanced" hidden>
                ${this.input('phase', 'Carrier phase', '°', '', '')}
                ${this.input('duty', 'Pulse duty', '%', '0', '100')}
                ${this.input('seed', 'Seed', '', '1', '4294967295', '1')}
                <button type="button" data-action="restart" title="Restart carrier and modulation phase; reseed noise and PRBS">Restart phase</button>
                <span class="pm-source-note"></span>
            </div></fieldset><div class="pm-status" role="status" aria-live="polite" hidden></div>
        </section>`;
    }

    private channel(channel: number): HTMLElement {
        return this.root.querySelector(`[data-channel="${channel}"]`);
    }

    private render(channel: number): void {
        const section = this.channel(channel);
        const settings = this.values[channel];
        const scales = {carrier: 1e6, modulation: 1e3, duty: .01};
        for (const field of ['carrier', 'phase', 'modulation', 'deviation', 'duty', 'seed']) {
            if (field === 'carrier' || field === 'modulation') {
                this.frequencies[channel][field].setValue(settings[field]);
                continue;
            }
            const input = section.querySelector<HTMLInputElement>(`[data-field="${field}"]`);
            const value = settings[field] / (scales[field] || 1);
            const turn = Math.pow(2, this.info[channel].phaseWidth);
            const resolution = field === 'seed' ? 0 : field === 'duty' ? 100 / turn : 360 / turn;
            // Display the shortest value consistent with half a hardware LSB.
            // A requested 10 kHz or 1° stays readable after DDS quantization.
            let displayed = value;
            for (let digits = 1; digits <= 15; digits++) {
                const candidate = Number(value.toPrecision(digits));
                if (Math.abs(candidate - value) <= resolution / 2) { displayed = candidate; break; }
            }
            input.value = String(displayed);
            input.title = `Accepted: ${value}`;
            input.setCustomValidity('');
            input.removeAttribute('aria-invalid');
        }
        const pm = section.querySelector<HTMLInputElement>('[data-field="pm"]');
        pm.checked = settings.pm;
        const waveform = section.querySelector<HTMLSelectElement>('[data-field="waveform"]');
        // PM can be off while its saved source has been compiled out.
        if (!Array.from(waveform.options).some(option => Number(option.value) === settings.waveform)) {
            const option = section.ownerDocument.createElement('option');
            option.value = String(settings.waveform);
            option.textContent = `${this.shapes[settings.waveform]} (unavailable)`;
            option.disabled = true;
            waveform.appendChild(option);
        }
        waveform.value = String(settings.waveform);
        const output = section.querySelector<HTMLButtonElement>('[data-action="output"]');
        output.setAttribute('aria-pressed', String(settings.output));
        output.textContent = settings.output ? 'On' : 'Muted';
        output.setAttribute('aria-label', `${settings.output ? 'Mute' : 'Enable'} DAC ${channel} output`);
        section.dataset.output = settings.output ? 'on' : 'muted';
        this.enableFields(channel);
    }

    private enableFields(channel: number): void {
        const section = this.channel(channel);
        const settings = this.values[channel];
        const enabled = !!this.info[channel].capabilities;
        // Leave source available while PM is off so users can prepare settings.
        section.querySelector<HTMLSelectElement>('[data-field="waveform"]').disabled = !this.info[channel].capabilities;
        for (const field of ['modulation', 'deviation']) {
            section.querySelector<HTMLInputElement>(`[data-field="${field}"]`).disabled = !enabled;
        }
        section.querySelector<HTMLInputElement>('[data-field="duty"]').disabled = settings.waveform !== 2;
        section.querySelector<HTMLInputElement>('[data-field="seed"]').disabled = [6, 7, 8].indexOf(settings.waveform) < 0;
        const note = section.querySelector<HTMLElement>('.pm-source-note');
        note.textContent = settings.waveform === 9 ? 'BPSK: phase shift between two states' :
            settings.waveform === 8 ? `PRBS · PN${this.info[channel].prbsWidth}` :
            settings.waveform === 0 ? 'Sine amplitude is ± the selected angle' : '';
    }

    private status(channel: number, text: string, error: boolean = false): void {
        const node = this.channel(channel).querySelector<HTMLElement>('.pm-status');
        node.textContent = text;
        node.hidden = !text;
        node.dataset.state = error ? 'error' : 'busy';
        node.setAttribute('role', error ? 'alert' : 'status');
    }

    private message(error: any): string { return error instanceof Error ? error.message : String(error); }

    private perform(channel: number, action: () => Promise<void>, pendingText: string = 'Applying…', queue = false): Promise<{settings?: PhaseModulatorSettings; error: string}> {
        if (this.disposed || (this.busy[channel] && !queue)) { return Promise.resolve({error: 'Control is unavailable.'}); }
        const operation = this.busy[channel] ? this.operations[channel].then(() => this.apply(channel, action, pendingText)) :
            this.apply(channel, action, pendingText);
        this.operations[channel] = operation.then(() => {});
        return operation;
    }

    private async apply(channel: number, action: () => Promise<void>, pendingText: string): Promise<{settings?: PhaseModulatorSettings; error: string}> {
        if (this.disposed) { return {error: 'Control is closed.'}; }
        this.busy[channel] = true;
        const section = this.channel(channel);
        // Readonly retains focus and tab order while an acknowledgement arrives.
        // Disabling the entire fieldset would drop keyboard focus on each edit.
        section.querySelector('fieldset').setAttribute('aria-busy', 'true');
        section.querySelectorAll<HTMLInputElement>('input[type="number"]').forEach(input => { input.readOnly = true; });
        this.root.querySelector('[data-action="refresh"]').setAttribute('aria-disabled', 'true');
        this.status(channel, pendingText);
        let failure = '';
        let settings: PhaseModulatorSettings;
        try {
            await action();
        } catch (error) { failure = this.message(error); }
        try {
            settings = await this.driver.settings(channel);
            if (this.disposed) { return {error: 'Control is closed.'}; }
            this.values[channel] = settings;
            this.render(channel);
        } catch (error) { failure = failure || this.message(error); }
        if (this.disposed) { return {error: 'Control is closed.'}; }
        this.busy[channel] = false;
        section.querySelector('fieldset').setAttribute('aria-busy', 'false');
        section.querySelectorAll<HTMLInputElement>('input[type="number"]').forEach(input => { input.readOnly = false; });
        this.root.querySelector('[data-action="refresh"]').setAttribute('aria-disabled', String(this.busy.some(Boolean)));
        this.status(channel, failure, !!failure);
        return {settings, error: failure};
    }

    private change(event: Event): void {
        const input = event.target as HTMLInputElement;
        const field = input.dataset.field as PhaseModulatorField;
        if (!field) { return; }
        if (field === 'carrier' || field === 'modulation') { return; } // FrequencyInput owns editing and tuning.
        const channel = Number(input.closest('[data-channel]').getAttribute('data-channel'));
        if (this.busy[channel]) {
            event.preventDefault();
            this.render(channel);
            return;
        }
        let value: number | boolean = input.type === 'checkbox' ? input.checked : Number(input.value);
        if (input.type === 'number') {
            input.setCustomValidity('');
            const scales = {carrier: 1e6, modulation: 1e3, duty: .01};
            value = input.valueAsNumber * (scales[field] || 1);
            if (field === 'seed' && this.values[channel].waveform === 8 &&
                Number.isInteger(value) && (value % Math.pow(2, this.info[channel].prbsWidth)) === 0) {
                input.setCustomValidity(`Seed must be nonzero within PN${this.info[channel].prbsWidth}.`);
            }
            if (!Number.isFinite(value) || !input.checkValidity()) {
                input.setAttribute('aria-invalid', 'true');
                input.reportValidity();
                this.status(channel, input.validationMessage || 'Enter a valid value.', true);
                return;
            }
            input.removeAttribute('aria-invalid');
        }
        void this.perform(channel, () => this.driver.set(channel, field, value));
    }

    private click(event: Event): void {
        const button = (event.target as HTMLElement).closest<HTMLButtonElement>('button[data-action]');
        if (!button || !this.root.contains(button)) { return; }
        const action = button.dataset.action;
        if (action === 'retry') { void this.init().catch(() => {}); return; }
        if (action === 'refresh') {
            if (this.busy.some(Boolean)) { return; }
            this.info.forEach((_, channel) => { void this.perform(channel, async () => {}, 'Reading…'); });
            return;
        }
        const channel = Number(button.closest('[data-channel]').getAttribute('data-channel'));
        if (action === 'more') {
            const expanded = button.getAttribute('aria-expanded') !== 'true';
            button.setAttribute('aria-expanded', String(expanded));
            this.channel(channel).querySelector<HTMLElement>('.pm-advanced').hidden = !expanded;
            return;
        }
        if (action === 'output') {
            void this.perform(channel, () => this.driver.set(channel, 'output', !this.values[channel].output));
        } else if (action === 'restart') {
            void this.perform(channel, () => this.driver.restart(channel));
        }
    }

    dispose(): void {
        this.disposed = true;
        this.root.removeEventListener('change', this.changeHandler);
        this.root.removeEventListener('click', this.clickHandler);
        this.frequencies.forEach(controls => { controls.carrier.dispose(); controls.modulation.dispose(); });
        this.root.querySelectorAll('fieldset').forEach(fieldset => { fieldset.disabled = true; });
    }
}
