// Temperature readouts use one decimal; hosts own transport and polling.
function updateTemperatureReadouts(spans: ArrayLike<HTMLElement>, values: ArrayLike<number>): void {
    for (let i = 0; i < spans.length; i++) {
        const span = spans[i];
        const text = values[Number(span.dataset.index)].toFixed(1);
        if (span.textContent !== text) { span.textContent = text; }
    }
}
