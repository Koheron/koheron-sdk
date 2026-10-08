// Shared stale-readout presentation; hosts own shutdown and socket disposal.
function showPnaConnectionError(document: Document): void {
    const status = document.getElementById('connection-status');
    const wasConnected = status.dataset.state === 'live';
    status.textContent = 'Disconnected';
    status.dataset.state = 'error';
    document.getElementById('connection-error').hidden = false;
    const message = document.getElementById('connection-error-message');
    if (message) { message.textContent = wasConnected
        ? 'Connection lost. The spectrum and readings are stale.'
        : 'Unable to connect to the phase-noise analyzer.'; }
    const average = document.getElementById('average-status');
    if (average) {
        average.textContent = '—/';
        average.dataset.state = 'unknown';
        average.title = 'Disconnected';
        average.setAttribute('aria-label', 'Average progress unavailable: disconnected');
    }
    document.querySelectorAll('.carrier-power-span, .phase-jitter-span, .time-jitter-span, #jitter-range, .tracking-state, .tracking-effective-bandwidth, .tracking-correction-0, .tracking-correction-1, .tracking-correction-x, .tracking-correction-y, #decade-values-table tbody td:last-child')
        .forEach(node => { node.textContent = '—'; });
    for (const [id, text, title] of [
        ['performance-status', 'Queue —', 'Processing status unavailable while disconnected'],
        ['coverage-status', 'Coverage —', 'Coverage unavailable while disconnected'],
        ['precision-status', '—', 'Acquisition status unavailable while disconnected']
    ]) {
        const node = document.getElementById(id);
        if (node) {
            node.textContent = text;
            node.dataset.state = 'unknown';
            node.title = title;
        }
    }
}
