class SystemInfoWidget {
    constructor(private document: Document, runtime?: RuntimeStream) {
        document.getElementById('system-info-retry').addEventListener('click', () => this.load());
        this.load();
        if (runtime) {
            runtime.subscribe(status => this.renderHealth(status.health), error => {
                const status = document.getElementById('health-status');
                status.textContent = 'Disconnected · last known readings'; status.title = error;
                status.dataset.state = 'error'; status.hidden = false;
                document.getElementById('health-table').dataset.stale = 'true';
            });
        }
    }

    private renderHealth(health: any): void {
        const table = this.document.getElementById('health-table') as HTMLTableElement;
        const status = this.document.getElementById('health-status');
        status.textContent = 'Live'; status.title = ''; status.hidden = false; status.dataset.state = 'ready';
        table.dataset.stale = 'false';
        const uptimeSeconds = health.uptime_seconds;
        const uptime = Number.isFinite(uptimeSeconds) && uptimeSeconds >= 0 ?
            uptimeSeconds < 60 ? `${Math.floor(uptimeSeconds)} s` : uptimeSeconds < 3600 ? `${Math.floor(uptimeSeconds / 60)} min` :
            `${Math.floor(uptimeSeconds / 3600)}h ${Math.floor(uptimeSeconds / 60) % 60}m` : 'Unavailable';
        const memory = health.memory || {}, disks = health.storage || {}, service = health.instrument_service || {};
        const extraction = health.timing?.extraction || {};
        const elapsed = extraction.ExecMainExitTimestampMonotonic - extraction.ExecMainStartTimestampMonotonic;
        const seconds = (value: number) => Number.isFinite(value) && value > 0 ?
            value < 100 ? '<0.1 ms' : value < 1000000 ? `${(value / 1000).toFixed(1)} ms` : `${(value / 1000000).toFixed(2)} s` : 'Unavailable';
        const values = [
            uptime, Array.isArray(health.load_average) && health.load_average.length ? health.load_average.map((n: number) => n.toFixed(2)).join(' · ') : 'Unavailable',
            PreflightView.bytes(memory.available_bytes), PreflightView.bytes(memory.total_bytes),
            PreflightView.bytes(disks.instruments?.available_bytes) + ' free',
            PreflightView.bytes(disks.staging?.available_bytes) + ' free',
            service.state || 'Unavailable', service.result || 'Unavailable',
            seconds(health.timing?.api_initialization_us),
            seconds(service.ActiveEnterTimestampMonotonic - service.ExecMainStartTimestampMonotonic),
            seconds(elapsed)
        ];
        const signature = JSON.stringify(values);
        if (table.dataset.signature === signature) { return; }
        table.dataset.signature = signature;
        for (const [index, value] of values.entries()) {
            const cell = table.rows[index].cells[1];
            cell.textContent = value; cell.title = value;
        }
    }

    private async load(): Promise<void> {
        const status = this.document.getElementById('system-info-status');
        const retry = this.document.getElementById('system-info-retry');
        retry.hidden = true; status.hidden = false;
        status.textContent = 'Loading…'; status.dataset.state = 'loading';
        try {
            const data = await new KoheronSystem().getBuildSummary();
            const release = data.release || {} as ReleaseData;
            const manifest = data.manifest || {} as ManifestData;
            this.document.getElementById('board-name').textContent = manifest.board ? manifest.board.toUpperCase() : '';
            const table = this.document.getElementById('release-table') as HTMLTableElement;
            table.textContent = '';
            const fields: Record<string, string> = { ...release, board: manifest.board,
                kernel: manifest.kernel, zynq: manifest.zynq, generated_utc: manifest.generated_utc };
            for (const key of ['board', ...KoheronSystem.releaseDisplayOrder]) {
                if (!fields[key]) { continue; }
                const row = table.insertRow();
                const label = this.document.createElement('th');
                label.scope = 'row';
                label.textContent = key === 'board' ? 'Board' : KoheronSystem.releaseDisplayLabels[key] || key;
                row.appendChild(label);
                row.insertCell().textContent = fields[key];
            }
            status.textContent = ''; status.dataset.state = 'ready'; status.hidden = true;
        } catch (_) {
            status.textContent = 'Cannot load system information.'; status.dataset.state = 'error';
            retry.hidden = false;
        }
    }
}
