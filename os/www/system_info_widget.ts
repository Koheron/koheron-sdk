class SystemInfoWidget {
    constructor(private document: Document, runtime?: RuntimeStream) {
        document.getElementById('system-info-retry').addEventListener('click', () => this.load());
        this.load();
        if (runtime) {
            runtime.subscribe(status => this.renderHealth(status.health), error => {
                const status = document.getElementById('health-status'); status.textContent = error; status.dataset.state = 'error'; status.hidden = false;
                document.getElementById('health-table').dataset.stale = 'true';
            });
        }
    }

    private renderHealth(health: any): void {
        const table = this.document.getElementById('health-table') as HTMLTableElement;
        const status = this.document.getElementById('health-status'); status.hidden = true; status.dataset.state = 'ready';
        table.dataset.stale = 'false';
        const uptime = Number.isFinite(health.uptime_seconds) ? `${Math.floor(health.uptime_seconds / 3600)}h ${Math.floor(health.uptime_seconds / 60) % 60}m` : 'Unavailable';
        const memory = health.memory || {}, disks = health.storage || {}, service = health.instrument_service || {};
        const extraction = health.timing?.extraction || {};
        const elapsed = extraction.ExecMainExitTimestampMonotonic - extraction.ExecMainStartTimestampMonotonic;
        const seconds = (value: number) => value > 0 ? `${(value / 1000000).toFixed(2)} s` : 'Unavailable';
        const fields: Array<[string, string]> = [
            ['Uptime', uptime], ['Load · 1/5/15m', Array.isArray(health.load_average) && health.load_average.length ? health.load_average.map((n: number) => n.toFixed(2)).join(' · ') : 'Unavailable'],
            ['RAM available', PreflightView.bytes(memory.available_bytes)], ['RAM total', PreflightView.bytes(memory.total_bytes)],
            ['Instrument storage', PreflightView.bytes(disks.instruments?.available_bytes) + ' free'],
            ['Staging storage', PreflightView.bytes(disks.staging?.available_bytes) + ' free'],
            ['Instrument service', service.state || 'Unavailable'],
            ['Service result', service.result || 'Unavailable'],
            ['API initialization', seconds(health.timing?.api_initialization_us)],
            ['Server startup', seconds(service.ActiveEnterTimestampMonotonic - service.ExecMainStartTimestampMonotonic)],
            ['Boot extraction', seconds(elapsed)]
        ];
        const signature = JSON.stringify(fields);
        if (table.dataset.signature === signature) { return; }
        table.dataset.signature = signature; table.textContent = '';
        for (const [label, value] of fields) {
            const row = table.insertRow(), heading = this.document.createElement('th');
            heading.scope = 'row'; heading.textContent = label; row.appendChild(heading); row.insertCell().textContent = value;
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
