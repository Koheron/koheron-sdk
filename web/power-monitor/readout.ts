// Render the shared supply payload without owning transport or polling.
function updateSupplyReadouts(spans: ArrayLike<HTMLElement>, values: ArrayLike<number>): void {
    for (let i = 0; i < spans.length; i++) {
        const span = spans[i];
        const value = values[Number(span.dataset.index || '0')];
        const text = span.dataset.type === 'voltage' ? value.toFixed(3)
            : span.dataset.type === 'current' ? (value * 1000).toFixed(1) : '';
        if (span.textContent !== text) { span.textContent = text; }
    }
}
