class SystemInfoWidget {
    constructor(private document: Document) {
        document.getElementById('system-info-retry').addEventListener('click', () => this.load());
        this.load();
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
