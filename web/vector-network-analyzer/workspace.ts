class VNAWorkspace {
    private static readonly DEMO_NOTICE = 'Synthetic demo. Measurements and calibration here are simulated.';
    readonly model: VNAModel;
    readonly plot: VNAPlot;
    private referencePanel: PlotReferencePanel<VNATrace>;
    private client: Client | null = null;
    private driver: VNADriver | null = null;
    private worker: Worker | null = null;
    private connected = false;
    private busy = false;
    private calibrating = false;
    private disposed = false;
    private token = 0;
    private generation = 0;
    private repeatTimer: number | null = null;
    private stopTimer: number | null = null;
    private events = new InstrumentEvents();
    private storageKey: string;
    constructor(private window: Window, private document: Document, readonly demo = false) {
        document.title = `Vector network analyzer · ${VNAProfile.label} · Koheron`;
        this.el('board-name').textContent = VNAProfile.label;
        this.el('connection-detail').textContent = `${VNAModel.FS / 1e6} MS/s · logarithmic chirp`;
        this.el('empty-state').textContent = `Connect ${VNAProfile.output} → device → ${VNAProfile.input}, then run a sweep or record a thru.`;
        this.el('connection-notes').textContent = `${VNAProfile.output} → DUT → ${VNAProfile.input}. Keep the same cables and load after calibration. Source level is digital dBFS.`;
        this.el('thru-connectors').textContent = `${VNAProfile.output} directly to ${VNAProfile.input}`;
        this.input('stop-frequency').max = String(VNAProfile.maxFrequency / 1e6);
        this.input('start-frequency').max = String(VNAProfile.maxFrequency / 1000);
        this.model = new VNAModel(demo ? 'demo' : 'hardware');
        // Keep the original ALPHA250 key for existing sessions.
        this.storageKey = `koheron-vna-v1-${VNAProfile.board === 'alpha250' ? '' : VNAProfile.board + '-'}${demo ? 'demo' : 'hardware'}`;
        try {
            const saved = window.localStorage.getItem(this.storageKey);
            if (saved) this.model.load(saved);
        } catch { this.message('The saved session could not be restored. You can load an exported JSON session.'); }
        this.plot = new VNAPlot(this.model, document, () => this.renderMarkers());
        this.referencePanel = new PlotReferencePanel(document, this.model.referenceCollection,
            () => { this.persist(); this.render(); }, item => {
                try { this.model.replaceReference(item); }
                catch (error) { this.message(error.message, true); }
            }, item => {
                const s = item.frame.settings;
                const text = `${item.calibrated ? 'Thru corrected' : 'Uncalibrated'} · ${s.level} dBFS`;
                return {text, title: `${text} · ${vnaFrequency(s.start)}–${vnaFrequency(s.stop)} · ${item.frame.frequency.length} points · ${item.frame.source}`};
            });
        this.writeSettings();
        this.writeDisplaySettings();
        this.bindAcquisition();
        this.bindDisplay();
        this.bindMarkers();
        this.bindExports();
        const exportMenu = this.document.querySelector('.export-menu') as HTMLDetailsElement;
        this.events.listen(document, 'pointerdown', event => {
            if (!exportMenu.contains(event.target as Node)) exportMenu.open = false;
        });
        this.events.listen(document, 'keydown', (event: KeyboardEvent) => {
            if (event.key === 'Escape' && exportMenu.open) {
                exportMenu.open = false; (exportMenu.querySelector('summary') as HTMLElement).focus();
            }
        });
        this.events.listen(window, 'pagehide', () => this.dispose());
        this.events.listen(window, 'pageshow', (e: PageTransitionEvent) => { if (e.persisted) window.location.reload(); });
        this.render();
        if (demo) {
            this.connected = true;
            this.status('Synthetic demo', 'demo');
            this.el('connection-detail').textContent = 'Simulated data · no hardware commands';
            this.message(VNAWorkspace.DEMO_NOTICE);
            this.render();
        } else this.connect();
    }
    private bindAcquisition(): void {
        this.events.listen(this.el('sweep-form'), 'submit', e => { e.preventDefault(); this.start(false); });
        this.events.listen(this.el('sweep-form'), 'change', () => this.readSettings());
        this.events.listen(this.el('run'), 'click', () => this.start(false));
        this.events.listen(this.el('stop'), 'click', () => this.stop());
        this.events.listen(this.el('retry'), 'click', () => this.connect());
        this.events.listen(this.el('repeat'), 'change', () => { if (!this.checked('repeat')) this.clearRepeat(); });
        this.events.listen(this.el('calibrate'), 'click', () => {
            if (this.readSettings()) (this.el('calibration-dialog') as HTMLDialogElement).showModal();
        });
        this.events.listen(this.el('confirm-calibration'), 'click', () => {
            (this.el('calibration-dialog') as HTMLDialogElement).close(); this.start(true);
        });
        this.events.listen(this.el('apply-calibration'), 'change', () => {
            this.model.applyCalibration = this.checked('apply-calibration'); this.persist(); this.render();
        });
        this.events.listen(this.el('clear-calibration'), 'click', () => {
            this.model.calibration = null; this.persist(); this.render();
        });
    }
    private bindDisplay(): void {
        this.events.listen(this.el('unwrap'), 'change', () => { this.model.unwrap = this.checked('unwrap'); this.persist(); this.render(); });
        this.events.listen(this.el('port-extension'), 'change', () => {
            const input = this.input('port-extension'), value = Number(input.value);
            const valid = input.value.trim() !== '' && Number.isFinite(value) && Math.abs(value) <= 1e6;
            input.setAttribute('aria-invalid', String(!valid));
            if (valid) { this.model.delayNs = value; this.persist(); this.render(); }
            else this.message('Port extension must be a finite delay between −1,000,000 and 1,000,000 ns.', true);
        });
        for (const [id, delay] of [['view-phase', false], ['view-delay', true]] as [string, boolean][]) {
            this.events.listen(this.el(id), 'click', () => {
                this.plot.delayView = delay;
                this.el('view-phase').setAttribute('aria-pressed', String(!delay));
                this.el('view-delay').setAttribute('aria-pressed', String(delay));
                this.el('secondary-title').textContent = delay ? 'Group delay' : 'Transmission phase';
                this.el('secondary-unit').textContent = delay ? 'ns' : 'degrees';
                this.el('phase-plot').setAttribute('aria-label', `${delay ? 'Group delay' : 'Transmission phase'}. Frequency zoom is shared with magnitude.`);
                this.el('unwrap-label').hidden = delay; this.render();
            });
        }
    }
    private bindMarkers(): void {
        this.events.listen(this.el('reset-view'), 'click', () => this.plot.reset());
        this.events.listen(this.el('find-peak'), 'click', () => this.findPeak());
        this.events.listen(this.el('find-cutoff'), 'click', () => this.findCutoff());
        for (let i = 0; i < 3; ++i) this.events.listen(this.el(`marker-${i}`), 'change', () => {
            const input = this.input(`marker-${i}`);
            const match = input.value.trim().match(/^([\d.]+)\s*(hz|khz|mhz)?$/i);
            const frequency = match ? Number(match[1]) * ({hz:1, khz:1e3, mhz:1e6}[match[2]?.toLowerCase() || 'hz']) : NaN;
            if (!Number.isFinite(frequency) || frequency <= 0 || frequency > VNAProfile.maxFrequency) {
                input.setAttribute('aria-invalid', 'true'); this.message('Enter a marker frequency such as 10 MHz or 100 kHz.', true); return;
            }
            input.setAttribute('aria-invalid', 'false'); this.plot.markers[i] = frequency; this.renderMarkers(); this.plot.draw();
        });
    }
    private bindExports(): void {
        this.events.listen(this.el('capture-reference'), 'click', () => {
            try { this.model.captureReference(); }
            catch (error) { this.message(error.message, true); }
        });
        this.events.listen(this.el('export-csv'), 'click', () => this.download(this.model.csv(), 'text/csv', 'measurement.csv'));
        this.events.listen(this.el('export-session'), 'click', () => this.download(this.model.serialize(), 'application/json', 'session.json'));
        this.events.listen(this.el('export-png'), 'click', () => {
            this.plot.image().toBlob(blob => { if (blob) this.download(blob, 'image/png', 'response.png'); });
        });
        this.events.listen(this.el('import-session'), 'click', () => this.input('session-file').click());
        this.events.listen(this.el('session-file'), 'change', async () => {
            const input = this.input('session-file'), file = input.files?.[0]; if (!file) return;
            try {
                if (this.busy) throw new Error('Finish or stop the active sweep before loading a session.');
                if (file.size > 2 * 1024 * 1024) throw new Error('Session file is too large. Maximum size is 2 MiB.');
                const text = await file.text();
                if (this.disposed) return;
                if (this.busy) throw new Error('Finish or stop the active sweep before loading a session.');
                const next = new VNAModel(this.demo ? 'demo' : 'hardware'); next.load(text);
                this.model.load(next.serialize()); this.writeSettings(); this.writeDisplaySettings(); this.plot.reset(); this.persist(); this.render();
                this.message('Session loaded. The source remains off until you run a sweep.');
            } catch (error) { this.message(error.message, true); }
            finally { input.value = ''; }
        });
    }
    private el(id: string): HTMLElement { return this.document.getElementById(id); }
    private input(id: string): HTMLInputElement { return this.el(id) as HTMLInputElement; }
    private checked(id: string): boolean { return this.input(id).checked; }
    private status(text: string, state: string): void {
        this.el('connection-status').textContent = text; this.el('connection-status').dataset.state = state;
    }
    private message(text: string, error = false): void {
        const element = this.el('message'); element.textContent = text; element.hidden = !text;
        this.el('progress-detail').hidden = !!text;
        element.dataset.state = error ? 'error' : 'info';
    }
    private async connect(): Promise<void> {
        if (this.demo || this.disposed) return;
        this.client?.exit(); this.connected = false; this.driver = null;
        this.status('Connecting', 'connecting'); this.el('offline-actions').hidden = true; this.render();
        const client = new Client(this.window.location.hostname, 1, () => {
            if (this.disposed || this.client !== client) return;
            this.connected = false; this.status('Disconnected', 'error');
            this.message('Connection lost. The chirp returns the output to zero automatically.', true);
            this.el('offline-actions').hidden = false; if (this.busy) this.stop(); this.render();
        });
        this.client = client;
        try {
            await client.init(); if (this.disposed || this.client !== client) return;
            this.driver = new VNADriver(client); this.connected = true; this.status('Connected', 'live'); this.message('');
        } catch (error) {
            if (this.disposed || this.client !== client) return;
            client.exit(); this.status('Disconnected', 'error'); this.message(error.message, true); this.el('offline-actions').hidden = false;
        }
        this.render();
    }
    private writeSettings(): void {
        const s = this.model.settings;
        this.input('start-frequency').value = String(s.start / 1000);
        this.input('stop-frequency').value = String(s.stop / 1e6);
        const points = this.el('point-count') as HTMLSelectElement;
        if (!Array.from(points.options).some(o => Number(o.value) === s.points)) points.add(new Option(String(s.points), String(s.points)));
        points.value = String(s.points);
        const duration = this.el('sweep-duration') as HTMLSelectElement;
        if (!Array.from(duration.options).some(o => Number(o.value) === s.duration)) duration.add(new Option(`${s.duration * 1000} ms`, String(s.duration)));
        duration.value = String(s.duration); this.input('source-level').value = String(s.level);
        (this.el('record-mode') as HTMLSelectElement).value = s.record || 'full';
        this.limitDurations(s.start, s.stop);
    }
    private limitDurations(start: number, stop: number): void {
        if (!(start > 0 && stop > start)) return;
        const min = VNAModel.minimumDuration(start, stop);
        Array.from((this.el('sweep-duration') as HTMLSelectElement).options).forEach(option => option.disabled = Number(option.value) < min);
    }
    private writeDisplaySettings(): void {
        this.input('apply-calibration').checked = this.model.applyCalibration;
        this.input('unwrap').checked = this.model.unwrap;
        this.input('port-extension').value = String(this.model.delayNs);
    }
    private readSettings(updateView = true): boolean {
        try {
            const ids = ['start-frequency', 'stop-frequency', 'source-level'];
            if (ids.some(id => this.input(id).value.trim() === '')) throw new Error('Complete every sweep field before measuring.');
            this.limitDurations(Number(this.input('start-frequency').value) * 1e3, Number(this.input('stop-frequency').value) * 1e6);
            const s = VNAModel.validateSettings({start: Number(this.input('start-frequency').value) * 1e3,
                stop: Number(this.input('stop-frequency').value) * 1e6,
                points: Number((this.el('point-count') as HTMLSelectElement).value),
                duration: Number((this.el('sweep-duration') as HTMLSelectElement).value), level: Number(this.input('source-level').value),
                record: (this.el('record-mode') as HTMLSelectElement).value as VNASettings['record']});
            ids.forEach(id => this.input(id).setAttribute('aria-invalid', 'false'));
            this.model.settings = s; this.persist(); if (updateView) this.render(); return true;
        } catch (error) { this.message(error.message, true); return false; }
    }
    async start(calibration: boolean): Promise<void> {
        if (this.busy || !this.connected || this.disposed || !this.readSettings(false)) return;
        this.clearRepeat(); this.busy = true; this.calibrating = calibration; this.token = 0;
        const generation = ++this.generation, settings = {...this.model.settings};
        this.message(this.demo ? VNAWorkspace.DEMO_NOTICE : '');
        this.progress('Preparing sweep', 0); this.render(false);
        if (this.demo) {
            await new Promise(resolve => this.window.setTimeout(resolve, 350));
            if (this.disposed || generation !== this.generation || !this.busy) return;
            const frame = this.demoFrame(settings, calibration);
            this.accept(frame); return;
        }
        try {
            const worker = this.worker || new Worker('chirp-worker.js'); this.worker = worker;
            worker.onmessage = event => {
                if (this.disposed || generation !== this.generation) return;
                const data = event.data;
                if (data.type === 'token') this.token = data.token;
                else if (data.type === 'progress') this.progress(data.stage, data.fraction);
                else if (data.type === 'result') {
                    try {
                        this.accept(VNAModel.validateFrame(data.frame));
                        const timing = this.el('sweep-timing');
                        timing.textContent = `${(data.timing.total / 1000).toFixed(2)} s · ${data.timing.bytes / 1048576} MiB`;
                        timing.title = `Capture ${(data.timing.capture / 1000).toFixed(2)} s · Download ${(data.timing.download / 1000).toFixed(2)} s · Reference ${data.timing.referenceCached ? 'reused' : (data.timing.reference / 1000).toFixed(2) + ' s'} · Response ${(data.timing.response / 1000).toFixed(2)} s`;
                        timing.dataset.timing = JSON.stringify(data.timing);
                    }
                    catch (error) { this.finish(error.message); }
                } else if (data.type === 'cancelled') this.finish('Sweep cancelled.');
                else if (data.type === 'error') this.finish(data.message, true);
            };
            worker.onerror = () => {
                if (!this.disposed && generation === this.generation) {
                    worker.terminate(); this.worker = null;
                    this.finish('The measurement worker stopped. Close other tabs and retry.', true);
                }
            };
            worker.postMessage({type: 'measure', settings, host: this.window.location.hostname});
        } catch (error) { this.finish(error.message, true); }
    }
    private accept(frame: VNAFrame): void {
        if (this.calibrating) {
            if (frame.real.some((r, i) => r === null || frame.imag[i] === null || Math.hypot(r, frame.imag[i]) < 1e-6)) {
                this.finish(`Thru response is too weak or incomplete. Check the direct ${VNAProfile.output}–${VNAProfile.input} connection.`, true); return;
            }
            this.model.calibration = frame; this.model.applyCalibration = true; this.input('apply-calibration').checked = true;
            this.finish('Thru recorded. Insert the device, keep the cables and settings, then run a sweep.');
        } else {
            const previous = this.model.latest;
            this.model.latest = frame;
            if (previous && (previous.settings.start !== frame.settings.start || previous.settings.stop !== frame.settings.stop)) this.plot.reset();
            this.finish();
        }
        this.persist();
    }
    private finish(message = '', error = false): void {
        if (this.disposed) return;
        this.busy = false;
        if (this.stopTimer !== null) this.window.clearTimeout(this.stopTimer); this.stopTimer = null;
        const token = this.token; this.token = 0;
        if (token && this.driver) this.driver.release(token).catch(() => {});
        this.el('progress-panel').dataset.state = 'idle';
        this.el('progress-title').textContent = error ? 'Sweep failed' : message === 'Sweep cancelled.' ? 'Sweep cancelled' : 'Sweep complete';
        this.el('progress-detail').textContent = 'Source output is off · ready for the next sweep.';
        if (message) this.message(message, error);
        this.render();
        if (!message && !this.calibrating && this.checked('repeat') && this.connected)
            this.repeatTimer = this.window.setTimeout(() => this.start(false), 16);
    }
    stop(): void {
        this.clearRepeat(); this.input('repeat').checked = false;
        // Keep one fallback per sweep so finish() can cancel it completely.
        if (!this.busy || this.stopTimer !== null) return;
        if (this.demo) { ++this.generation; this.finish('Sweep cancelled.'); return; }
        this.worker?.postMessage({type: 'cancel'}); this.el('progress-title').textContent = 'Stopping sweep';
        this.stopTimer = this.window.setTimeout(() => {
            ++this.generation; this.worker?.terminate(); this.worker = null; this.finish('Sweep cancelled.');
        }, 2500);
    }
    private clearRepeat(): void {
        if (this.repeatTimer !== null) this.window.clearTimeout(this.repeatTimer); this.repeatTimer = null;
    }
    private progress(stage: string, fraction: number): void {
        this.el('progress-panel').dataset.state = 'busy'; this.el('progress-title').textContent = stage;
        this.el('progress-percent').textContent = `${Math.round(fraction * 100)}%`;
        (this.el('sweep-progress') as HTMLProgressElement).value = fraction;
        this.el('progress-detail').textContent = fraction < .30 ? 'The source is active only during the chirp.' : 'Processing in the browser · source output is off';
    }
    private persist(): void { try { this.window.localStorage.setItem(this.storageKey, this.model.serialize()); } catch {} }
    private render(drawPlots = true): void {
        const latest = this.model.latest;
        const n = VNAModel.recordSamples(this.model.settings);
        this.el('board-notes').textContent = `${VNAProfile.label} · 1 kHz–${VNAProfile.maxFrequency / 1e6} MHz · ${VNAProfile.mutedOutput} muted · ${(n / VNAModel.FS * 1000).toFixed(1)} ms ADC record · ${(VNAModel.FS / n).toFixed(2)} Hz spacing`;
        (this.el('run') as HTMLButtonElement).disabled = !this.connected || this.busy;
        (this.el('calibrate') as HTMLButtonElement).disabled = !this.connected || this.busy;
        (this.el('sweep-settings') as HTMLFieldSetElement).disabled = this.busy;
        (this.el('stop') as HTMLButtonElement).disabled = !this.busy;
        ['export-csv', 'export-png', 'find-peak', 'find-cutoff'].forEach(id => (this.el(id) as HTMLButtonElement).disabled = !latest);
        (this.el('capture-reference') as HTMLButtonElement).disabled = !latest || this.model.references.length >= PlotReferences.limit;
        this.document.querySelectorAll<HTMLButtonElement>('.replace-reference').forEach(button => button.disabled = !latest);
        (this.el('clear-reference') as HTMLButtonElement).disabled = !this.model.references.length;
        (this.el('apply-calibration') as HTMLInputElement).disabled = !this.model.calibration || this.busy;
        (this.el('clear-calibration') as HTMLButtonElement).disabled = !this.model.calibration || this.busy;
        const cal = this.model.calibration, match = this.model.calibrationMatches();
        this.el('calibration-badge').textContent = cal ? (match ? 'READY' : 'SETTINGS CHANGED') : 'NOT RECORDED';
        this.el('calibration-badge').className = `badge ${cal && match ? 'green' : 'amber'}`;
        this.el('calibration-description').textContent = cal ? (match ? 'Thru recorded for these sweep settings. Keep the same cables and connections.' : 'Record a new thru or restore the calibrated sweep settings to apply correction.') : 'Remove the board and cable response with a direct loopback measurement.';
        this.el('calibration-date').textContent = cal ? `Recorded ${new Date(cal.timestamp).toLocaleString()}` : '';
        this.el('calibrate').textContent = cal ? 'Record new thru' : 'Record thru';
        const corrected = latest && this.model.display(latest).calibrated;
        this.el('correction-label').textContent = corrected ? 'Thru corrected' : 'Uncalibrated';
        this.el('correction-label').className = `badge ${corrected ? 'green' : 'neutral'}`;
        this.el('measurement-summary').textContent = latest ? `${latest.source === 'demo' ? 'SIMULATED · ' : ''}${latest.frequency.length} points · ${vnaFrequency(latest.settings.start)}–${vnaFrequency(latest.settings.stop)} · ${latest.settings.level} dBFS · ${new Date(latest.timestamp).toLocaleTimeString()}` : 'No measurement yet';
        this.el('measurement-summary').title = this.el('measurement-summary').textContent;
        this.renderMarkers(); if (drawPlots) this.plot.draw();
    }
    private renderMarkers(): void {
        const data = this.model.latest ? this.model.display(this.model.latest) : null;
        for (let marker = 0; marker < 3; ++marker) {
            const input = this.input(`marker-${marker}`);
            if (this.document.activeElement !== input && input.getAttribute('aria-invalid') !== 'true') input.value = vnaFrequency(this.plot.markers[marker]);
            const i = data ? this.plot.nearest(data.frame.frequency, this.plot.markers[marker]) : -1;
            for (const [key, values, unit] of [['gain', data?.magnitude, 'dB'], ['phase', data?.phase, '°'], ['delay', data?.delay, 'ns']] as [string, number[], string][]) {
                const cell = this.el(`marker-${marker}-${key}`);
                cell.textContent = data && Number.isFinite(values[i]) ? `${values[i].toFixed(2)} ${unit}` : '—';
                cell.title = cell.textContent;
            }
            if (data) input.title = `Nearest acquired point: ${vnaFrequency(data.frame.frequency[i])}`;
        }
    }
    private findPeak(): void {
        if (!this.model.latest) return;
        const d = this.model.display(this.model.latest);
        let index = -1; d.magnitude.forEach((v, i) => { if (Number.isFinite(v) && (index < 0 || v > d.magnitude[index])) index = i; });
        if (index >= 0) { this.plot.markers[0] = d.frame.frequency[index]; this.renderMarkers(); this.plot.draw(); }
    }
    private findCutoff(): void {
        if (!this.model.latest) return;
        const d = this.model.display(this.model.latest), max = Math.max(...d.magnitude.filter(Number.isFinite)), target = max - 3;
        const crossings: number[] = [];
        for (let i = 1; i < d.magnitude.length; ++i) {
            const a = d.magnitude[i-1], b = d.magnitude[i];
            if (Number.isFinite(a) && Number.isFinite(b) && ((a >= target && b < target) || (a < target && b >= target))) crossings.push(d.frame.frequency[i]);
        }
        if (!crossings.length) { this.message('No −3 dB crossing relative to the peak is present in this sweep.'); return; }
        this.plot.markers[1] = crossings[0]; if (crossings.length > 1) this.plot.markers[2] = crossings[crossings.length-1]; this.renderMarkers(); this.plot.draw();
    }
    private download(content: string | Blob, type: string, suffix: string): void {
        const url = URL.createObjectURL(typeof content === 'string' ? new Blob([content], {type}) : content);
        const link = this.document.createElement('a'); link.href = url; link.download = `${VNAProfile.board}-vna-${suffix}`; link.click();
        this.window.setTimeout(() => URL.revokeObjectURL(url), 1000);
        (this.document.querySelector('.export-menu') as HTMLDetailsElement).open = false;
    }
    private demoFrame(settings: VNASettings, thru: boolean): VNAFrame {
        const recordSamples = VNAModel.recordSamples(settings);
        const frequency = VNAModel.frequencyBins(settings, recordSamples).map(bin => bin * VNAModel.FS / recordSamples);
        const real: number[] = [], imag: number[] = [];
        frequency.forEach(f => {
            const r = f / 24e6, base = .96 / Math.sqrt(1 + (f / 140e6) ** 2), delay = -2 * Math.PI * f * 7e-9;
            let amplitude = base, phase = delay;
            if (!thru) { amplitude *= .985 / Math.hypot(1-r*r, Math.SQRT2*r); phase -= Math.atan2(Math.SQRT2*r, 1-r*r); }
            real.push(amplitude * Math.cos(phase)); imag.push(amplitude * Math.sin(phase));
        });
        return VNAModel.validateFrame({source: 'demo', settings, recordSamples, timestamp: new Date().toISOString(), frequency, real, imag});
    }
    dispose(): void {
        if (this.disposed) return;
        this.disposed = true; ++this.generation; this.clearRepeat();
        if (this.stopTimer !== null) this.window.clearTimeout(this.stopTimer);
        this.worker?.terminate(); this.worker = null;
        if (this.token && this.driver) this.driver.release(this.token).catch(() => {}).finally(() => this.client?.exit());
        else this.client?.exit();
        this.referencePanel.dispose(); this.plot.dispose(); this.events.dispose();
    }
}
