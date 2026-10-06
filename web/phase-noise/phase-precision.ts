// Precision is latched by the FPGA for each complete acquisition packet.
class PhasePrecision {
    private id: number;
    private commands: Commands;
    private select: HTMLSelectElement;
    private status: HTMLElement;
    private timer: number;
    private coverage: HTMLElement;
    private performanceStatus: HTMLElement;
    private coverageEpoch: number;
    private coverageHistory: {time: number; covered: number; span: number}[] = [];
    private stopped = false;
    private changing = false;

    constructor(private client: Client, document: Document) {
        const driver = client.getDriver('PhaseNoiseAnalyzer');
        this.id = driver.id;
        this.commands = driver.getCmds();
        this.select = document.getElementById('phase-precision') as HTMLSelectElement;
        this.status = document.getElementById('precision-status');
        this.coverage = document.getElementById('coverage-status');
        this.performanceStatus = document.getElementById('performance-status');
        this.select.addEventListener('change', this.change);
    }

    async init(): Promise<void> {
        await this.refresh();
        this.schedule();
    }

    private change = async (): Promise<void> => {
        this.changing = true;
        this.select.disabled = true;
        try {
            const accepted = await this.client.readBool(
                Command(this.id, this.commands['set_phase_precision'], Number(this.select.value)));
            if (!accepted) { throw new Error('Unsupported phase precision.'); }
            await this.refresh();
        } catch (error) {
            if (!this.stopped) {
                this.status.textContent = 'Change failed';
                this.status.title = 'Unable to change phase precision. Retry the selection.';
                this.status.dataset.state = 'error';
            }
        } finally {
            this.changing = false;
            if (!this.stopped) { this.select.disabled = false; }
        }
    };

    private async refresh(): Promise<void> {
        // Read uint64 counters as pairs to avoid depending on BigInt support.
        const values = await this.client.readTuple(
            Command(this.id, this.commands['get_precision_status']), 'IIdIIIIIIIdd');
        let overruns = 0;
        if (this.commands['get_stream_status'] !== undefined) {
            const stream = await this.client.readTuple(
                Command(this.id, this.commands['get_stream_status']), 'IIIIIII');
            // RPC integers are network byte order: high uint32 precedes low.
            overruns = 4294967296 * stream[2] + stream[3];
        }
        if (this.stopped) { return; }
        const [requested, captured, step, state] = values;
        await this.refreshCoverage(state);
        await this.refreshPerformance(state);
        if (this.stopped) { return; }
        // A pending user choice takes precedence over an older polling response.
        if (!this.changing) { this.select.value = String(requested); }
        const resolution = step >= 1e-3 ? `${(step * 1e3).toPrecision(4)} mrad`
                                     : `${(step * 1e6).toPrecision(4)} µrad`;
        const messages = ['Settling…', 'Live', 'Overrange', 'Read error', 'Sample gap'];
        this.status.textContent = `${resolution} · ${state === 1 && overruns && this.commands['get_stream_coverage'] === undefined ? 'Live · Skips' : messages[state] || 'Waiting…'}`;
        this.status.dataset.state = state > 1 ? 'error' : state === 1 ? 'live' : 'waiting';
        const detail = state === 2
            ? requested > 0 ? 'Reduce precision or bring the LO closer to the carrier.'
                            : 'Bring the LO closer to the carrier.'
            : state === 4 ? 'ADC samples were lost; this capture was discarded and acquisition is restarting.'
            : state === 3 ? 'Acquisition error; reconnect or restart the instrument if it persists.'
            : 'Higher precision reduces the available phase range.';
        const coverage = overruns
            ? ` ${overruns} consumer overruns since instrument start: FFT windows were skipped, while valid averages were retained. Increase decimation for continuous coverage.`
            : '';
        this.status.title = `${resolution} per count. Requested +${requested} bits; last packet +${captured} bits. ${detail}${coverage}`;
    }

    private async refreshPerformance(state: number): Promise<void> {
        const badge = this.performanceStatus;
        if (!badge) { return; }
        if (this.commands['get_stream_performance'] === undefined) {
            badge.hidden = true;
            return;
        }
        badge.hidden = false;
        const values = await this.client.readTuple(
            Command(this.id, this.commands['get_stream_performance']), 'ddddddddd');
        if (this.stopped) { return; }
        const [total, fft, average, publication, copy, queue, retention, required, capacity] = values;
        if (state !== 1 || values.some(value => !Number.isFinite(value)) || capacity <= 0) {
            badge.textContent = 'Queue —';
            badge.dataset.state = 'waiting';
            badge.title = 'Waiting for processing capacity and queue measurements.';
            return;
        }
        const queued = queue < 1000 ? `${Math.round(queue)} ms` : `${(queue / 1000).toFixed(2)} s`;
        badge.textContent = `Queue ${queued}`;
        badge.dataset.state = capacity < required || queue > retention / 2 ? 'warning' : 'live';
        badge.title = `Queued phase-stream time: ${queued} of approximately ${(retention / 1000).toFixed(2)} s buffer capacity. Estimated processing capacity: ${capacity.toFixed(1)} windows/s; required: ${required.toFixed(1)} windows/s at 50% overlap. Smoothed service time: ${total.toFixed(2)} ms, including DMA copy ${copy.toFixed(2)}, FFT ${fft.toFixed(2)}, averaging ${average.toFixed(2)}, spectrum/jitter publication ${publication.toFixed(2)} ms. Acquisition and averaging continue on every accepted window; displayed spectra publish at up to 30/s. Capacity excludes waiting for incoming samples and may vary with board load.`;
        if (this.commands['get_fft_performance'] !== undefined) {
            const stages = await this.client.readTuple(
                Command(this.id, this.commands['get_fft_performance']), 'ddddd');
            if (!this.stopped && stages.every(value => Number.isFinite(value))) {
                const [fit, prepare, transform, reduction, wait] = stages;
                badge.title += ` FFT stage times per segment (paired channel maxima): fit ${fit.toFixed(2)}, window preparation ${prepare.toFixed(2)}, transform ${transform.toFixed(2)}, density reduction ${reduction.toFixed(2)}, paired wait ${wait.toFixed(2)} ms.`;
            }
        }
    }

    private async refreshCoverage(state: number): Promise<void> {
        if (!this.coverage) { return; }
        if (this.commands['get_stream_coverage'] === undefined) {
            this.coverage.textContent = 'Coverage n/a';
            this.coverage.dataset.state = 'unknown';
            this.coverage.title = 'Sample coverage is unavailable on this instrument version.';
            return;
        }
        const values = await this.client.readTuple(
            Command(this.id, this.commands['get_stream_coverage']), 'IIIIII');
        if (this.stopped) { return; }
        const u64 = (index: number) => 4294967296 * values[index] + values[index + 1];
        const epoch = u64(0), covered = u64(2), span = u64(4);
        if (epoch !== this.coverageEpoch) {
            this.coverageEpoch = epoch;
            this.coverageHistory = [];
            // Counters start at the first accepted FFT window in this epoch.
            this.coverageHistory.push({time: performance.now(), covered: 0, span: 0});
        }
        if (state !== 1 || span === 0) {
            this.coverage.textContent = 'Coverage —';
            this.coverage.dataset.state = 'waiting';
            this.coverage.title = 'Waiting for valid acquisitions. Target: 100% sample coverage.';
            return;
        }
        const now = performance.now();
        this.coverageHistory.push({time: now, covered, span});
        // Keep the point just before the 10-second boundary for stable deltas.
        while (this.coverageHistory.length > 2 && this.coverageHistory[1].time <= now - 10000) {
            this.coverageHistory.shift();
        }
        const first = this.coverageHistory[0];
        const recentSpan = span - first.span;
        if (recentSpan <= 0) {
            this.coverage.textContent = 'Coverage —';
            this.coverage.dataset.state = 'waiting';
            this.coverage.title = 'Waiting for another acquisition to measure recent coverage.';
            return;
        }
        const percent = Math.max(0, Math.min(100, 100 * (covered - first.covered) / recentSpan));
        const rounded = Math.round(percent * 10) / 10;
        this.coverage.textContent = `Coverage ${rounded === 100 ? '100' : rounded.toFixed(1)}%`;
        this.coverage.dataset.state = rounded === 100 ? 'live' : 'warning';
        this.coverage.title = `Target: 100%. Recent phase-stream sample coverage (~10 s of updates), counting FFT overlap once. Queued data is excluded until analyzed or skipped. Since acquisition reset: ${(100 * covered / span).toFixed(1)}% covered, ${(100 * (span - covered) / span).toFixed(1)}% skipped. FPGA sample loss is reported separately.`;
    }

    private schedule(): void {
        if (this.stopped) { return; }
        this.timer = window.setTimeout(async () => {
            try {
                if (!this.changing) { await this.refresh(); }
            } catch (error) {
                if (!this.stopped) {
                    if (this.performanceStatus) {
                        this.performanceStatus.textContent = 'Queue —';
                        this.performanceStatus.dataset.state = 'unknown';
                        this.performanceStatus.title = 'Processing status unavailable; retrying automatically.';
                    }
                    if (this.coverage) {
                        this.coverage.textContent = 'Coverage —';
                        this.coverage.dataset.state = 'unknown';
                        this.coverage.title = 'Coverage unavailable; retrying automatically.';
                    }
                    this.status.textContent = 'Status unavailable';
                    this.status.title = 'Unable to read phase precision. Retrying automatically.';
                    this.status.dataset.state = 'error';
                }
            } finally { this.schedule(); }
        }, 500);
    }

    dispose(): void {
        this.stopped = true;
        window.clearTimeout(this.timer);
        this.select.removeEventListener('change', this.change);
    }
}
