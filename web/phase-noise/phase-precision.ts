// Precision is latched by the FPGA for each complete acquisition packet.
class PhasePrecision {
    private id: number;
    private commands: Commands;
    private select: HTMLSelectElement;
    private status: HTMLElement;
    private timer: number;
    private stopped = false;
    private changing = false;

    constructor(private client: Client, document: Document, driverName = 'PhaseNoiseAnalyzer') {
        const driver = client.getDriver(driverName);
        this.id = driver.id;
        this.commands = driver.getCmds();
        this.select = document.getElementById('phase-precision') as HTMLSelectElement;
        this.status = document.getElementById('precision-status');
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
        // A pending user choice takes precedence over an older polling response.
        if (!this.changing) { this.select.value = String(requested); }
        const resolution = step >= 1e-3 ? `${(step * 1e3).toPrecision(4)} mrad`
                                     : `${(step * 1e6).toPrecision(4)} µrad`;
        const messages = ['Settling…', 'Live', 'Overrange', 'Read error', 'Sample gap'];
        this.status.textContent = `${resolution} · ${state === 1 && overruns ? 'Live · Skips' : messages[state] || 'Waiting…'}`;
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

    private schedule(): void {
        if (this.stopped) { return; }
        this.timer = window.setTimeout(async () => {
            try {
                if (!this.changing) { await this.refresh(); }
            } catch (error) {
                if (!this.stopped) {
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
