// Canvas views of the received spectrum history. No claim of gap-free RF capture.
class SpectrumViews {
    mode = 'spectrum';
    private canvas: HTMLCanvasElement;
    private texture: HTMLCanvasElement;
    private overlay: HTMLCanvasElement;
    private pointer: {clientX: number; clientY: number};
    private image: ImageData;
    private pixels: Uint32Array;
    private ordered: ImageData;
    private orderedPixels: Uint32Array;
    private rowPixels: Uint32Array[];
    private palette: Uint32Array;
    private paletteCSS: string[];
    private label = '';
    private key = '';
    private lastBucket = -Infinity;
    private lastVersion = -1;
    private low = -160;
    private high = -40;
    private unit = '';
    private manual = false;
    private bounds = {left: 54, top: 22, width: 1, height: 1};
    private drag: number;
    private bins: {first: number; last: number}[];
    private hits: Uint16Array;
    private densityY: number[];
    private resize: ResizeObserver;

    constructor(private document: Document, private history: SpectrumHistory,
        private range: () => {from: number; to: number},
        private convert: (power: number, unit: string) => number,
        private selectRange: (from: number, to: number) => void,
        private redraw: () => void) {
        this.canvas = document.getElementById('history-canvas') as HTMLCanvasElement;
        this.texture = document.createElement('canvas');
        this.overlay = document.getElementById('history-overlay') as HTMLCanvasElement;
        const bytes = new Uint8Array(256 * 4);
        const stops = [[0, 22, 38, 95], [.3, 0, 125, 185], [.55, 45, 200, 180], [.8, 245, 215, 55], [1, 225, 55, 45]];
        bytes.set([247, 250, 252, 255], 0);
        for (let i = 1; i < 256; i++) {
            const t = (i - 1) / 254;
            let s = 1; while (s < stops.length - 1 && stops[s][0] < t) { s++; }
            const a = stops[s - 1], b = stops[s], f = (t - a[0]) / (b[0] - a[0]);
            for (let j = 0; j < 3; j++) { bytes[i * 4 + j] = Math.round(a[j + 1] + f * (b[j + 1] - a[j + 1])); }
            bytes[i * 4 + 3] = 255;
        }
        this.palette = new Uint32Array(bytes.buffer);
        this.paletteCSS = Array.from({length: 256}, (_, i) => 'rgb(' + bytes[i * 4] + ',' + bytes[i * 4 + 1] + ',' + bytes[i * 4 + 2] + ')');
        const selector = document.getElementById('spectrum-view') as HTMLSelectElement;
        const setView = () => {
            this.mode = selector.value || 'spectrum';
            document.getElementById('plot-placeholder').hidden = this.mode !== 'spectrum';
            document.getElementById('plot-basics').hidden = this.mode !== 'spectrum';
            document.getElementById('history-view').hidden = this.mode === 'spectrum';
            document.getElementById('history-controls').hidden = this.mode === 'spectrum';
            document.getElementById('reference-actions').hidden = this.mode !== 'spectrum';
            document.getElementById('trace-controls').hidden = this.mode !== 'spectrum';
            document.getElementById('peak-detection').hidden = this.mode !== 'spectrum';
            this.canvas.setAttribute('aria-label', this.mode === 'spectrogram' ? 'Frequency versus time spectrogram' : 'Frequency versus level occurrence density');
            document.getElementById('history-cursor').hidden = this.mode === 'spectrum';
            document.getElementById('history-cursor').textContent = '';
            this.pointer = undefined; this.drag = undefined; this.drawOverlay();
            this.key = ''; this.redraw();
        };
        selector.addEventListener('change', setView);
        selector.disabled = false;
        setView();
        document.getElementById('history-duration').addEventListener('change', () => {
            this.history.setDuration(Number((document.getElementById('history-duration') as HTMLSelectElement).value));
            this.key = ''; this.redraw();
        });
        const auto = document.getElementById('auto-level') as HTMLInputElement;
        auto.addEventListener('change', () => {
            this.manual = !auto.checked; this.key = ''; this.redraw();
        });
        for (const id of ['color-low', 'color-high']) {
            const updateLevels = () => {
                const lo = document.getElementById('color-low') as HTMLInputElement;
                const hi = document.getElementById('color-high') as HTMLInputElement;
                lo.setCustomValidity(''); hi.setCustomValidity('');
                if (!Number.isFinite(lo.valueAsNumber) || !Number.isFinite(hi.valueAsNumber)) {
                    if (!Number.isFinite(lo.valueAsNumber)) { lo.setCustomValidity('Enter a finite level'); }
                    if (!Number.isFinite(hi.valueAsNumber)) { hi.setCustomValidity('Enter a finite level'); }
                    return;
                }
                if (lo.valueAsNumber >= hi.valueAsNumber) { lo.setCustomValidity('Low must be below High'); return; }
                this.low = lo.valueAsNumber; this.high = hi.valueAsNumber; this.key = ''; this.redraw();
            };
            document.getElementById(id).addEventListener('input', updateLevels);
            document.getElementById(id).addEventListener('change', updateLevels);
            document.getElementById(id).addEventListener('keydown', (event: KeyboardEvent) => {
                if (event.key === 'Enter') {
                    updateLevels(); (document.getElementById(id) as HTMLInputElement).reportValidity();
                } else if (event.key === 'Escape') {
                    for (const [field, value] of [['color-low', this.low], ['color-high', this.high]] as [string, number][]) {
                        const input = document.getElementById(field) as HTMLInputElement;
                        input.value = String(value); input.setCustomValidity('');
                    }
                    this.redraw();
                }
            });
        }
        this.canvas.addEventListener('pointerdown', e => {
            if (e.button !== 0 || !this.inside(e)) { return; }
            this.pointer = {clientX: e.clientX, clientY: e.clientY};
            this.drag = e.clientX - this.canvas.getBoundingClientRect().left;
            this.canvas.setPointerCapture(e.pointerId); this.drawOverlay();
        });
        this.canvas.addEventListener('pointerup', e => {
            const end = e.clientX - this.canvas.getBoundingClientRect().left;
            if (this.drag !== undefined && Math.abs(end - this.drag) > 6) {
                const range = this.range(), b = this.bounds;
                const x = (px: number) => range.from + Math.max(0, Math.min(1, (px - b.left) / b.width)) * (range.to - range.from);
                const from = x(Math.min(this.drag, end)), to = x(Math.max(this.drag, end));
                if (to > from) { this.selectRange(from, to); }
            }
            this.drag = undefined;
            this.pointer = this.inside(e) ? {clientX: e.clientX, clientY: e.clientY} : undefined;
            if (this.pointer) { this.inspect(this.pointer); }
            this.drawOverlay();
        });
        this.canvas.addEventListener('pointercancel', () => { this.drag = undefined; this.pointer = undefined; this.drawOverlay(); });
        this.canvas.addEventListener('dblclick', () => $('#plot-placeholder').trigger('dblclick'));
        this.canvas.addEventListener('pointermove', e => {
            this.pointer = this.inside(e) || this.drag !== undefined ? {clientX: e.clientX, clientY: e.clientY} : undefined;
            this.inspect(e); this.drawOverlay();
        });
        this.canvas.addEventListener('pointerleave', () => {
            if (this.drag === undefined) { this.pointer = undefined; this.drawOverlay(); }
            document.getElementById('history-cursor').textContent = '';
        });
        if (typeof ResizeObserver !== 'undefined') {
            this.resize = new ResizeObserver(() => { if (this.mode !== 'spectrum') { this.redraw(); } });
            this.resize.observe(this.canvas);
        }
    }

    private format(value: number): string { return Math.abs(value) >= 10000 || (Math.abs(value) > 0 && Math.abs(value) < .01) ? value.toExponential(1) : Number(value.toPrecision(4)).toString(); }
    private color(value: number): number {
        if (Number.isNaN(value)) { return 0; }
        return Math.max(1, Math.min(255, 1 + Math.round((value - this.low) / (this.high - this.low) * 254)));
    }
    private columns(range: {from: number; to: number}, width: number): {first: number; last: number}[] {
        const size = this.history.average.length, step = this.history.status.fs / (size * 2) / 1e6;
        return Array.from({length: width}, (_, x) => ({
            first: Math.max(0, Math.min(size - 1, Math.floor((range.from + x / width * (range.to - range.from)) / step))),
            last: Math.max(0, Math.min(size - 1, Math.ceil((range.from + (x + 1) / width * (range.to - range.from)) / step) - 1))
        }));
    }

    render(unit: string, label: string, paused = false): void {
        if (this.mode === 'spectrum') { return; }
        this.label = label.replace(/^.*\(/, '').replace(/\)/, '');
        const unitLabel = this.document.getElementById('history-level-unit');
        if (unitLabel.textContent !== this.label) { unitLabel.textContent = this.label; }
        if (unit !== this.unit) {
            this.unit = unit; this.manual = false;
            (this.document.getElementById('auto-level') as HTMLInputElement).checked = true;
        }

        const width = this.canvas.clientWidth, height = this.canvas.clientHeight;
        if (!width || !height) { return; }
        const scale = window.devicePixelRatio || 1;
        if (this.canvas.width !== Math.round(width * scale) || this.canvas.height !== Math.round(height * scale)) {
            this.canvas.width = Math.round(width * scale); this.canvas.height = Math.round(height * scale);
        }
        const ctx = this.canvas.getContext('2d'); ctx.setTransform(scale, 0, 0, scale, 0, 0);
        const b = this.bounds = {left: 54, top: 22, width: Math.max(1, width - 132), height: Math.max(1, height - 60)};
        if (this.overlay.width !== this.canvas.width || this.overlay.height !== this.canvas.height) {
            this.overlay.width = this.canvas.width; this.overlay.height = this.canvas.height; this.drawOverlay();
        }
        if (!this.history.samples) {
            const message = paused ? 'History cleared · Resume to collect spectra' : 'Waiting for received spectra…';
            ctx.fillStyle = 'white'; ctx.fillRect(0, 0, width, height);
            ctx.fillStyle = '#f7fafc'; ctx.fillRect(b.left, b.top, b.width, b.height);
            ctx.strokeStyle = '#d5d5d5'; ctx.strokeRect(b.left, b.top, b.width, b.height);
            ctx.font = '12px sans-serif'; ctx.textAlign = 'center'; ctx.fillStyle = '#666';
            ctx.fillText(paused ? 'History cleared' : 'Waiting for spectra…', b.left + b.width / 2, b.top + b.height / 2);
            if (paused) {
                ctx.font = '11px sans-serif';
                ctx.fillText('Resume to collect spectra', b.left + b.width / 2, b.top + b.height / 2 + 18);
            }
            this.document.getElementById('history-cursor').textContent = message;
            for (const id of ['color-low', 'color-high']) {
                const input = this.document.getElementById(id) as HTMLInputElement;
                input.disabled = true; input.setCustomValidity('');
                if (!this.manual) { input.value = ''; }
            }
            this.pointer = undefined; this.drag = undefined; this.drawOverlay(); return;
        }
        const cursor = this.document.getElementById('history-cursor');
        if (cursor.textContent === 'Waiting for received spectra…' || cursor.textContent === 'History cleared · Resume to collect spectra') { cursor.textContent = ''; }
        if (!this.manual) {
            const min = this.convert(this.history.minPower, unit), max = this.convert(this.history.maxPower, unit);
            if (unit === 'dBm-Hz' || unit === 'dBm') {
                this.low = Math.floor((min - 5) / 10) * 10; this.high = Math.ceil((max + 5) / 10) * 10;
            } else {
                this.low = Math.max(0, min * .9); this.high = Math.max(this.low + 1, max * 1.1);
            }
        }
        for (const [id, value] of [['color-low', this.low], ['color-high', this.high]] as [string, number][]) {
            const input = this.document.getElementById(id) as HTMLInputElement;
            input.disabled = !this.manual;
            if (!this.manual) {
                input.setCustomValidity('');
                const formatted = String(Number(value.toPrecision(5)));
                if (input.value !== formatted) { input.value = formatted; }
            }
        }
        const range = this.range(), columns = Math.min(1024, Math.ceil(b.width * scale));
        const rows = this.mode === 'spectrogram' ? Math.round(this.history.duration / this.history.interval) + 1 : 256;
        const key = [this.mode, this.history.epoch, unit, this.low, this.high, range.from, range.to, columns, rows].join('/');
        const changed = key !== this.key;
        if (changed) {
            this.key = key; this.texture.width = columns; this.texture.height = rows;
            this.image = this.texture.getContext('2d').createImageData(columns, rows);
            this.pixels = new Uint32Array(this.image.data.buffer); this.pixels.fill(this.palette[0]);
            if (this.mode === 'spectrogram') {
                this.ordered = this.texture.getContext('2d').createImageData(columns, rows);
                this.orderedPixels = new Uint32Array(this.ordered.data.buffer);
                this.rowPixels = Array.from({length: rows}, (_, row) => new Uint32Array(this.image.data.buffer, row * columns * 4, columns));
            } else { this.ordered = undefined; this.orderedPixels = undefined; this.rowPixels = undefined; }
            this.lastBucket = -Infinity; this.lastVersion = -1;
            this.bins = this.columns(range, columns);
            this.hits = new Uint16Array(columns * rows);
            this.densityY = Array.from({length: 256}, (_, code) => Math.floor((this.high - this.convert(SpectrumHistory.power(code), unit)) / (this.high - this.low) * (rows - 1)));
        }
        const bins = this.bins;
        if (this.mode === 'spectrogram') {
            const bucket = this.history.currentBucket;
            if (!changed && bucket > this.lastBucket) {
                for (let k = Math.max(this.lastBucket + 1, bucket - rows + 1); k <= bucket; k++) {
                    this.pixels.fill(this.palette[0], (k % rows) * columns, (k % rows + 1) * columns);
                }
            }
            const latest = this.history.rows[this.history.rows.length - 1];
            for (const row of this.history.rows) {
                if (row.bucket < bucket - rows + 1 || (!changed && row.bucket < this.lastBucket) || (!changed && row.bucket === this.lastBucket && row.version === this.lastVersion)) { continue; }
                const offset = (row.bucket % rows) * columns;
                for (let x = 0; x < columns; x++) {
                    let power = NaN;
                    for (let i = bins[x].first; i <= bins[x].last; i++) {
                        const value = row.psd[i]; if (Number.isFinite(value) && (!Number.isFinite(power) || value > power)) { power = value; }
                    }
                    this.pixels[offset + x] = this.palette[this.color(this.convert(power, unit))];
                }
            }
            this.lastBucket = bucket; this.lastVersion = latest.version;
        } else {
            const hits = this.hits; hits.fill(0);
            const y = this.densityY;
            for (let x = 0; x < columns; x++) {
                for (let i = bins[x].first; i <= bins[x].last; i++) {
                    for (let code = this.history.densityLow[i]; code <= this.history.densityHigh[i]; code++) {
                        const row = y[code]; if (row < 0 || row >= rows) { continue; }
                        const index = row * columns + x;
                        hits[index] = Math.max(hits[index], this.history.density[i * 256 + code]);
                    }
                }
            }
            const norm = this.history.densityFrames || 1;
            const colors = new Uint32Array(norm + 1); colors[0] = this.palette[0];
            for (let count = 1; count <= norm; count++) { colors[count] = this.palette[Math.max(1, Math.round(Math.log1p(100 * count / norm) / Math.log(101) * 254) + 1)]; }
            for (let i = 0; i < hits.length; i++) { this.pixels[i] = colors[hits[i]]; }
        }
        if (this.mode === 'spectrogram') {
            // Unwrap into one contiguous image before scaling. Two fractional
            // destination rectangles can leave a visible seam at the ring join.
            for (let age = 0; age < rows; age++) {
                const slot = ((this.lastBucket - age) % rows + rows) % rows;
                this.orderedPixels.set(this.rowPixels[slot], age * columns);
            }
        }
        this.texture.getContext('2d').putImageData(this.ordered || this.image, 0, 0);
        ctx.fillStyle = 'white'; ctx.fillRect(0, 0, width, height);
        ctx.imageSmoothingEnabled = false;
        if (this.mode === 'spectrogram') {
            // The newest bucket is only partly elapsed. Crop its future portion
            // and move the past rows continuously on every received frame.
            const phase = this.history.bucketPhase;
            ctx.drawImage(this.texture, 0, 1 - phase, columns, rows - 1,
                b.left, b.top, b.width, b.height);
        } else { ctx.drawImage(this.texture, b.left, b.top, b.width, b.height); }
        ctx.font = '11px sans-serif'; ctx.fillStyle = '#555'; ctx.strokeStyle = '#d5d5d5'; ctx.lineWidth = 1;
        ctx.strokeRect(b.left, b.top, b.width, b.height);
        ctx.textAlign = 'center'; ctx.fillText('Frequency (MHz)', b.left + b.width / 2, height - 6);
        for (let i = 0; i <= 4; i++) {
            const x = b.left + i * b.width / 4; ctx.fillText(this.format(range.from + i * (range.to - range.from) / 4), x, b.top + b.height + 16);
            ctx.textAlign = 'right'; ctx.fillText(this.format(this.mode === 'spectrogram' ? i * this.history.duration / 4 : this.high - i * (this.high - this.low) / 4), b.left - 6, b.top + i * b.height / 4 + 4); ctx.textAlign = 'center';
        }
        ctx.save(); ctx.translate(12, b.top + b.height / 2); ctx.rotate(-Math.PI / 2); ctx.fillText(this.mode === 'spectrogram' ? 'Age (s)' : label, 0, 0); ctx.restore();
        ctx.textAlign = 'left'; ctx.fillText(this.mode === 'spectrogram' ? this.history.duration + ' s · 50 ms peak rows' : this.history.duration + ' s · ' + this.history.densityFrames + ' received spectra', b.left, 13);
        const barX = b.left + b.width + 12;
        for (let y = 0; y < b.height; y++) {
            const index = Math.max(1, Math.round((1 - y / b.height) * 254) + 1);
            ctx.fillStyle = this.paletteCSS[index]; ctx.fillRect(barX, b.top + y, 8, 1);
        }
        ctx.fillStyle = '#555'; ctx.font = '10px sans-serif';
        if (this.mode === 'density') {
            ctx.fillText('Occurrence', barX, 13);
            for (const p of [100, 10, 1, .1]) { ctx.fillText(p + '%', barX + 12, b.top + (1 - Math.log1p(p) / Math.log(101)) * b.height + 4); }
        } else {
            ctx.fillText(label.replace(/^.*\(/, '').replace(/\)/, ''), barX, 13);
            for (let i = 0; i <= 4; i++) { ctx.fillText(this.format(this.high - i * (this.high - this.low) / 4), barX + 12, b.top + i * b.height / 4 + 4); }
        }
        if (this.pointer && this.drag === undefined) { this.inspect(this.pointer); }
    }

    dispose(): void { if (this.resize) { this.resize.disconnect(); } }

    private inside(e: {clientX: number; clientY: number}): boolean {
        const rect = this.canvas.getBoundingClientRect(), b = this.bounds;
        const x = e.clientX - rect.left, y = e.clientY - rect.top;
        return x >= b.left && x <= b.left + b.width && y >= b.top && y <= b.top + b.height;
    }

    private drawOverlay(): void {
        const ctx = this.overlay.getContext('2d'), scale = window.devicePixelRatio || 1;
        ctx.setTransform(scale, 0, 0, scale, 0, 0);
        ctx.clearRect(0, 0, this.overlay.width / scale, this.overlay.height / scale);
        if (!this.pointer || (this.drag === undefined && !this.inside(this.pointer))) { return; }
        const rect = this.canvas.getBoundingClientRect(), b = this.bounds;
        const x = Math.max(b.left, Math.min(b.left + b.width, this.pointer.clientX - rect.left));
        const y = Math.max(b.top, Math.min(b.top + b.height, this.pointer.clientY - rect.top));
        ctx.strokeStyle = 'rgba(1,156,213,.55)'; ctx.lineWidth = 1;
        if (this.drag !== undefined) {
            const start = Math.max(b.left, Math.min(b.left + b.width, this.drag));
            ctx.fillStyle = 'rgba(1,156,213,.12)'; ctx.fillRect(Math.min(start, x), b.top, Math.abs(x - start), b.height);
            ctx.strokeRect(Math.min(start, x), b.top, Math.abs(x - start), b.height);
        } else {
            ctx.beginPath(); ctx.moveTo(x, b.top); ctx.lineTo(x, b.top + b.height);
            ctx.moveTo(b.left, y); ctx.lineTo(b.left + b.width, y); ctx.stroke();
        }
    }

    private inspect(e: {clientX: number; clientY: number}): void {
        if (!this.history.samples) { return; }
        const rect = this.canvas.getBoundingClientRect(), b = this.bounds, range = this.range();
        const x = (e.clientX - rect.left - b.left) / b.width, y = (e.clientY - rect.top - b.top) / b.height;
        if (x < 0 || x > 1 || y < 0 || y > 1) { this.document.getElementById('history-cursor').textContent = ''; return; }
        const frequency = range.from + x * (range.to - range.from), step = this.history.status.fs / (this.history.average.length * 2) / 1e6;
        // Inspect the same texture cell used to draw the heatmap. A display
        // column can merge several FFT bins; report its strongest contributor.
        if (!this.bins || !this.texture.width) { return; }
        const column = Math.min(this.texture.width - 1, Math.floor(x * this.texture.width));
        const span = this.bins[column];
        let bin = Math.min(span.last, Math.max(span.first, Math.round(frequency / step)));
        let detail: string;
        if (this.mode === 'spectrogram') {
            const latest = this.history.currentBucket;
            const phase = this.history.bucketPhase;
            const bucket = latest - Math.floor(1 - phase + y * this.history.duration / this.history.interval);
            const row = this.history.rows.find(r => r.bucket === bucket);
            let power = NaN;
            if (row) {
                for (let i = span.first; i <= span.last; i++) {
                    const value = row.psd[i];
                    if (Number.isFinite(value) && (!Number.isFinite(power) || value > power)) { power = value; bin = i; }
                }
            }
            detail = (y * this.history.duration).toFixed(2) + ' s ago · ' +
                (Number.isFinite(power) ? this.format(this.convert(power, this.unit)) + ' ' + this.label : 'No received data');
        } else {
            const pixelY = Math.min(this.texture.height - 1, Math.floor(y * this.texture.height));
            let hits = 0, code = 0;
            for (let i = span.first; i <= span.last; i++) {
                for (let k = this.history.densityLow[i]; k <= this.history.densityHigh[i]; k++) {
                    const count = this.history.density[i * 256 + k];
                    if (this.densityY[k] === pixelY && count > hits) { hits = count; bin = i; code = k; }
                }
            }
            detail = hits ? this.format(this.convert(SpectrumHistory.power(code), this.unit)) + ' ' + this.label +
                ' · ' + hits + '/' + this.history.densityFrames + ' hits (' +
                (100 * hits / Math.max(1, this.history.densityFrames)).toFixed(2) + '%)' :
                this.format(this.high - y * (this.high - this.low)) + ' ' + this.label + ' · No occurrences';
        }
        const text = (bin * step).toFixed(6) + ' MHz · ' + detail;
        const cursor = this.document.getElementById('history-cursor');
        if (cursor.textContent !== text) { cursor.textContent = text; }
    }
}
