interface IMeasurements {
    phase_jitter: number; // rad rms
    time_jitter: number; // s rms
    freq_lo: number; // Integration interval start (Hz)
    freq_hi: number; // Integration interval end (Hz)
    carrier_power: number; // dBm
}

// Preserve each server's field precision while sharing the measurement shape.
async function readPnaMeasurements(client: Client, command: CmdMessage,
                                   format: 'ffffd' | 'fdddd'): Promise<IMeasurements> {
    const [phase_jitter, time_jitter, freq_lo, freq_hi, carrier_power] =
        await client.readTuple<[number, number, number, number, number]>(command, format);
    return {phase_jitter, time_jitter, freq_lo, freq_hi, carrier_power};
}

// Presentation only: acquisition and integration stay with the instrument.
class PnaMeasurementReadout {
    private power: HTMLElement;
    private phase: HTMLElement;
    private time: HTMLElement;
    private range: HTMLElement;

    constructor(document: Document) {
        this.power = document.querySelector('.carrier-power-span');
        this.phase = document.querySelector('.phase-jitter-span');
        this.time = document.querySelector('.time-jitter-span');
        this.range = document.getElementById('jitter-range');
    }

    private value(value: number, unit: string): string {
        if (!Number.isFinite(value)) { return '—'; }
        // Avoid presenting a rounded zero as negative noise or jitter.
        return `${(Math.abs(value) < .005 ? 0 : value).toFixed(2)} ${unit}`;
    }

    private frequency(value: number): string {
        const magnitude = Math.abs(value);
        const scale = magnitude >= 1e9 ? 1e9 : magnitude >= 1e6 ? 1e6 : magnitude >= 1e3 ? 1e3 : 1;
        const unit = scale === 1e9 ? 'GHz' : scale === 1e6 ? 'MHz' : scale === 1e3 ? 'kHz' : 'Hz';
        return `${Number((value / scale).toFixed(6))} ${unit}`;
    }

    clear(): void {
        for (const node of [this.power, this.phase, this.time, this.range]) {
            if (node) { node.textContent = '—'; node.removeAttribute('title'); }
        }
    }

    render(measurements: IMeasurements): void {
        this.power.textContent = this.value(measurements.carrier_power, 'dBm');
        this.phase.innerHTML = this.value(measurements.phase_jitter * 1e3, 'mrad <sub>rms</sub>');
        this.time.innerHTML = this.value(measurements.time_jitter * 1e12, 'ps <sub>rms</sub>');
        for (const node of [this.power, this.phase, this.time]) { node.title = node.textContent; }
        if (this.range) {
            const valid = Number.isFinite(measurements.freq_lo) && Number.isFinite(measurements.freq_hi);
            this.range.textContent = valid
                ? `${this.frequency(measurements.freq_lo)} – ${this.frequency(measurements.freq_hi)}` : '—';
            this.range.title = valid ? `${measurements.freq_lo} – ${measurements.freq_hi} Hz` : '';
        }
    }
}
