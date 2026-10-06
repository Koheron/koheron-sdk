interface DigitInputOptions {
    value: number;
    maximum: number;
    minimum?: number;
    inclusiveMaximum?: boolean;
    integer?: boolean;
    unitLabel?: string;
    units?: {[name: string]: number};
    frequency?: boolean;
    validate?: (value: number) => void;
    resolution: number; // Hardware LSB in the stored unit.
    step?: number; // Optional minimum tuning increment (e.g. even decimation).
    commit: (value: number) => Promise<number>; // Returns the accepted value.
    validation?: (message: string) => void; // Empty when the entry is corrected/cancelled.
}

// Text entry and digit tuning share one small control. It owns no transport.
class DigitInput {
    private accepted: number;
    private desired: number;
    private dirty = false;
    private tuning = false;
    private queued = false;
    private inFlight = false;
    private disposed = false;
    private exponent: number = 0; // Selected stored place value, survives carries.
    private minimumExponent: number;
    private timer: number;
    private wheelDelta = 0;
    private lastWheel = 0;
    private lastSent = 0;
    private removers: Array<() => void> = [];
    private hint: HTMLElement;
    private wheelHandler = (event: Event) => this.wheel(event as WheelEvent);

    private captureWheel(): void {
        this.input.ownerDocument.addEventListener('wheel', this.wheelHandler, {passive: false, capture: true});
    }

    private releaseWheel(): void {
        this.input.ownerDocument.removeEventListener('wheel', this.wheelHandler, true);
        this.wheelDelta = 0;
    }

    constructor(private input: HTMLInputElement, private unit: HTMLSelectElement,
                private options: DigitInputOptions) {
        this.accepted = this.desired = options.value;
        this.minimumExponent = Math.ceil(Math.log(options.resolution) / Math.LN10);
        this.hint = input.parentElement.querySelector('.pm-frequency-help');
        if (!this.hint) {
            this.hint = input.ownerDocument.createElement('span');
            this.hint.className = 'pm-frequency-help';
            this.hint.setAttribute('role', 'status');
            input.parentElement.appendChild(this.hint);
        }
        input.classList.add('digit-input');
        if (options.validation) { this.hint.dataset.validation = 'inline'; }
        input.setAttribute('role', 'spinbutton');
        if (Number.isFinite(options.minimum === undefined ? 0 : options.minimum)) {
            input.setAttribute('aria-valuemin', String(options.minimum === undefined ? 0 : options.minimum));
        }
        if (Number.isFinite(options.maximum)) {
            input.setAttribute('aria-valuemax', String(options.maximum - (options.inclusiveMaximum ? 0 : options.resolution)));
        }
        this.listen(input, 'input', () => {
            this.dirty = true;
            this.tuning = false;
            this.wheelDelta = 0;
            this.cancelQueued();
            this.clearError();
            this.help();
        });
        this.listen(input, 'change', () => {
            // Moving to the unit selector must let its choice finish typed entry.
            window.setTimeout(() => {
                if (!this.disposed && input.ownerDocument.activeElement !== unit) { this.commitEntry(); }
            }, 0);
        });
        this.listen(input, 'blur', event => {
            this.releaseWheel();
            if ((event as FocusEvent).relatedTarget !== unit) { this.commitEntry(); }
        });
        this.listen(unit, 'blur', event => {
            if ((event as FocusEvent).relatedTarget !== input) { this.commitEntry(); }
        });
        this.listen(input, 'focus', () => {
            this.tuning = !this.dirty;
            this.wheelDelta = 0;
            this.captureWheel();
            this.selectDigit();
            this.help();
        });
        this.listen(input, 'click', event => {
            if ((event as MouseEvent).detail > 1 || input.selectionEnd - input.selectionStart > 1) {
                this.tuning = false;
                this.wheelDelta = 0;
                this.help();
                return;
            }
            if (this.dirty) { return; }
            const start = this.clickedIndex(event as MouseEvent);
            const digits = this.digits();
            const nearest = digits.find(index => index >= start);
            this.exponent = Math.max(this.minimumExponent, this.power(nearest === undefined ? digits[digits.length - 1] : nearest));
            this.tuning = true;
            this.wheelDelta = 0;
            this.selectDigit();
            this.help();
        });
        this.listen(input, 'keydown', event => this.key(event as KeyboardEvent));
        this.listen(unit, 'change', () => {
            if (this.dirty) { this.commitEntry(true); }
            else { this.paint(); } // Display units never issue a command.
        });
        this.setValue(options.value);
    }

    private listen(target: HTMLElement, name: string, listener: EventListener, options?: AddEventListenerOptions): void {
        target.addEventListener(name, listener, options);
        this.removers.push(() => target.removeEventListener(name, listener, options));
    }

    private scale(): number { return (this.options.units || {[this.options.unitLabel || '']: 1})[this.unit.value]; }

    private clickedIndex(event: MouseEvent): number {
        const rect = this.input.getBoundingClientRect();
        if (!rect.width) { return this.input.selectionStart; }
        const style = this.input.ownerDocument.defaultView.getComputedStyle(this.input);
        const context = this.input.ownerDocument.createElement('canvas').getContext('2d');
        if (!context) { return this.input.selectionStart; }
        // Chrome can leave the font shorthand empty when additional font
        // properties are set. Measure with the actual longhand properties.
        context.font = `${style.fontStyle} ${style.fontWeight} ${style.fontSize} ${style.fontFamily}`;
        const width = this.input.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
        const textWidth = context.measureText(this.input.value).width;
        if (textWidth > width) { return this.input.selectionStart; }
        let left = rect.left + parseFloat(style.borderLeftWidth) + parseFloat(style.paddingLeft) + width - textWidth;
        for (let index = 0; index < this.input.value.length; index++) {
            left += context.measureText(this.input.value[index]).width;
            if (event.clientX < left) { return index; }
        }
        return this.input.value.length;
    }

    private parse(forceUnit = false): {value: number; unit: string} {
        let entry = this.input.value.replace(/[\s_]/g, '');
        if (!this.options.frequency) {
            const suffix = this.options.unitLabel || '';
            if (suffix && entry.toLowerCase().endsWith(suffix.toLowerCase())) { entry = entry.slice(0, -suffix.length); }
            if (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(entry)) { throw new Error('Enter a number.'); }
            const value = Number(entry);
            this.validate(value);
            return {value, unit: this.unit.value};
        }
        const match = /^([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?)(GHz|MHz|kHz|Hz|G|M|k)?$/i.exec(entry);
        if (!match) { throw new Error('Enter a number, optionally followed by Hz, kHz, MHz or GHz.'); }
        const suffix = (match[2] || '').toLowerCase();
        const names = {hz: 'Hz', khz: 'kHz', k: 'kHz', mhz: 'MHz', m: 'MHz', ghz: 'GHz', g: 'GHz'};
        const unit = forceUnit ? this.unit.value : names[suffix] || this.unit.value;
        const value = Number(match[1]) * this.options.units[unit];
        this.validate(value);
        return {value, unit};
    }

    private validate(value: number): void {
        const minimum = this.options.minimum === undefined ? 0 : this.options.minimum;
        if (!Number.isFinite(value) || value < minimum ||
            (this.options.inclusiveMaximum ? value > this.options.maximum : value >= this.options.maximum)) {
            if (this.options.frequency) {
                throw new Error(`Frequency must be nonnegative and ${this.options.inclusiveMaximum ? 'at most' : 'below'} ${this.options.maximum / 1e6} MHz.`);
            }
            throw new Error(`Enter a value from ${minimum} to ${this.options.maximum}${this.options.unitLabel ? ' ' + this.options.unitLabel : ''}.`);
        }
        if (this.options.integer && !Number.isInteger(value)) { throw new Error('Enter a whole number.'); }
        if (this.options.validate) { this.options.validate(value); }
    }

    private error(error: any, validation = false): void {
        const message = error instanceof Error ? error.message : String(error);
        this.input.setCustomValidity(message);
        this.input.setAttribute('aria-invalid', 'true');
        this.input.setAttribute('aria-description', message);
        this.hint.textContent = message;
        this.hint.dataset.state = 'error';
        if (validation && this.options.validation) { this.options.validation(message); }
    }

    private clearError(): void {
        this.input.setCustomValidity('');
        this.input.removeAttribute('aria-invalid');
        this.input.removeAttribute('aria-description');
        delete this.hint.dataset.state;
        if (this.options.validation) { this.options.validation(''); }
    }

    private commitEntry(forceUnit = false): void {
        if (this.disposed || this.input.matches(':disabled') || this.input.readOnly) { return; }
        // Browser change events can also follow a digit step; avoid duplicate writes.
        if (!this.dirty && this.input.value === this.formatted(this.desired)) { return; }
        try {
            const parsed = this.parse(forceUnit);
            this.unit.value = parsed.unit;
            this.dirty = false;
            this.desired = parsed.value;
            this.clearError();
            this.paint();
            this.schedule(true);
        } catch (error) { this.dirty = true; this.error(error, true); }
    }

    commit(): void { this.commitEntry(); }

    private key(event: KeyboardEvent): void {
        if (event.key === 'Escape') {
            event.preventDefault();
            this.tuning = false;
            this.wheelDelta = 0;
            this.cancelQueued();
            this.dirty = false;
            this.desired = this.accepted;
            this.clearError();
            this.paint();
            this.input.setSelectionRange(this.input.value.length, this.input.value.length);
        } else if (event.key === 'F2' || ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'a')) {
            this.tuning = false;
            this.wheelDelta = 0;
            if (event.key === 'F2') {
                event.preventDefault();
                this.input.select();
            }
            this.help();
        } else if (event.key === 'Enter') {
            event.preventDefault();
            this.commitEntry();
        } else if (!event.ctrlKey && !event.metaKey && !event.altKey && (event.key === 'ArrowUp' || event.key === 'ArrowDown')) {
            event.preventDefault();
            this.step(event.key === 'ArrowUp' ? 1 : -1);
        } else if (!this.dirty && !event.ctrlKey && !event.metaKey && !event.shiftKey &&
                   (event.key === 'ArrowLeft' || event.key === 'ArrowRight')) {
            event.preventDefault();
            const change = event.key === 'ArrowLeft' ? 1 : -1;
            const highest = Math.floor(Math.log(Math.max(Math.abs(this.desired), this.scale())) / Math.LN10 + 1e-12);
            this.exponent = Math.max(this.minimumExponent, Math.min(highest, this.exponent + change));
            this.tuning = true;
            this.wheelDelta = 0;
            this.paint(); // Right can reveal another decimal digit.
        } else if (event.key === 'Home' || event.key === 'End' ||
                   ((event.shiftKey || event.ctrlKey || event.metaKey) &&
                    (event.key === 'ArrowLeft' || event.key === 'ArrowRight'))) {
            this.tuning = false;
            this.wheelDelta = 0;
            this.help();
        }
    }

    private wheel(event: WheelEvent): void {
        if (this.input.ownerDocument.activeElement !== this.input || !this.tuning || this.dirty || event.ctrlKey || event.metaKey ||
            this.input.selectionEnd - this.input.selectionStart !== 1 || !event.deltaY) { return; }
        event.preventDefault();
        if (Date.now() - this.lastWheel > 250) { this.wheelDelta = 0; }
        this.lastWheel = Date.now();
        if (Math.sign(event.deltaY) !== Math.sign(this.wheelDelta)) { this.wheelDelta = 0; }
        this.wheelDelta += event.deltaY;
        // One normal mouse notch; accumulate small trackpad deltas instead of
        // treating every pixel event as another frequency change.
        if (event.deltaMode === 0 && Math.abs(this.wheelDelta) < 40) { return; }
        this.step(this.wheelDelta < 0 ? 1 : -1);
        this.wheelDelta = 0;
    }

    private step(direction: number): void {
        if (this.input.matches(':disabled') || this.input.readOnly || this.disposed) { return; }
        try {
            if (this.dirty) {
                const parsed = this.parse();
                this.unit.value = parsed.unit;
                this.desired = parsed.value;
                this.dirty = false;
            }
            // Decimal rounding prevents 0.1 Hz steps accumulating binary noise.
            const step = Math.max(Math.pow(10, this.exponent), this.options.step || 0);
            const value = Number((this.desired + direction * step).toPrecision(15));
            this.validate(value);
            this.desired = value;
            this.tuning = true;
            this.clearError();
            this.paint();
            this.schedule(false);
        } catch (error) { this.error(error, true); }
    }

    private schedule(immediate: boolean): void {
        this.queued = true;
        if (this.inFlight) { return; }
        window.clearTimeout(this.timer);
        const delay = immediate ? 0 : Math.max(0, 100 - (Date.now() - this.lastSent));
        this.timer = window.setTimeout(() => { void this.flush(); }, delay);
    }

    private async flush(): Promise<void> {
        if (!this.queued || this.inFlight || this.disposed) { return; }
        this.queued = false;
        this.inFlight = true;
        const sent = this.desired;
        this.lastSent = Date.now();
        try {
            const accepted = await this.options.commit(sent);
            if (this.disposed) { return; }
            this.accepted = accepted;
            if (!this.queued && !this.dirty) {
                // Keep the readable request if it differs only by DDS rounding.
                this.desired = Math.abs(accepted - sent) <= this.options.resolution / 2 ? sent : accepted;
                this.paint();
            }
        } catch (error) {
            if (this.disposed) { return; }
            this.cancelQueued(); // Never retry a failed or ambiguous hardware commit.
            if (!this.dirty) { this.desired = this.accepted; this.paint(); }
            this.error(error);
        } finally {
            this.inFlight = false;
            if (this.queued && !this.disposed) { this.schedule(false); }
        }
    }

    private cancelQueued(): void {
        window.clearTimeout(this.timer);
        this.queued = false;
    }

    private formatted(hz: number): string {
        const scale = this.scale();
        const unitExponent = Math.round(Math.log(scale) / Math.LN10);
        if (!this.options.frequency) {
            const rounded = Number((hz).toPrecision(15));
            // Keep plain decimal text so every visible digit can be tuned.
            const decimals = Math.max(0, -this.exponent, -this.minimumExponent + 1);
            const text = rounded.toFixed(Math.min(15, decimals)).replace(/(\.\d*?)0+$/, '$1').replace(/\.$/, '');
            if (!this.tuning || this.exponent >= 0) { return text; }
            const parts = text.split('.');
            return parts[0] + '.' + ((parts[1] || '') + '000000000000000').slice(0, Math.max((parts[1] || '').length, Math.min(15, -this.exponent)));
        }
        const base = this.unit.value === 'MHz' ? 6 : this.unit.value === 'kHz' ? 3 : this.unit.value === 'GHz' ? 9 : 0;
        let decimals = Math.max(base, unitExponent - this.exponent, 0);
        let text: string;
        // Keep fine settings visible, even when they need more than the default digits.
        for (; decimals <= Math.max(base, unitExponent - this.minimumExponent + 1); decimals++) {
            text = (hz / scale).toFixed(decimals);
            if (Math.abs(Number(text) * scale - hz) <= this.options.resolution / 2) { break; }
        }
        const parts = text.split('.');
        const integer = parts[0].replace(/\B(?=(\d{3})+(?!\d))/g, '\u2009');
        const fraction = (parts[1] || '').replace(/(\d{3})(?=\d)/g, '$1\u2009');
        return integer + (parts.length > 1 ? '.' + fraction : '');
    }

    private paint(): void {
        const text = this.formatted(this.desired);
        // An acknowledgement must not disturb Ctrl+A, drag selection or the
        // caret when the displayed value is already correct.
        if (this.input.value !== text) { this.input.value = text; }
        this.input.setAttribute('aria-valuenow', String(this.desired));
        this.input.setAttribute('aria-valuetext', `${this.desired / this.scale()} ${this.unit.value}`.trim());
        this.input.title = `Accepted: ${this.accepted} ${this.options.frequency ? 'Hz' : this.options.unitLabel || ''}. Click a digit; scroll or use ↑/↓ to tune. ${this.options.frequency ? 'Type a value with optional units' : 'Type a value'}; Enter applies, Escape cancels.`;
        this.selectDigit();
        this.help();
    }

    private digits(): number[] {
        const result = [];
        for (let index = 0; index < this.input.value.length; index++) {
            if (/\d/.test(this.input.value[index])) { result.push(index); }
        }
        return result;
    }

    private power(index: number): number {
        const text = this.input.value;
        const point = text.indexOf('.') < 0 ? text.length : text.indexOf('.');
        const places = index < point ? (text.slice(index + 1, point).match(/\d/g) || []).length :
            -(text.slice(point + 1, index + 1).match(/\d/g) || []).length;
        return places + Math.round(Math.log(this.scale()) / Math.LN10);
    }

    private selectDigit(): void {
        if (this.input.ownerDocument.activeElement !== this.input || !this.tuning || this.dirty) { return; }
        const positions = this.digits();
        const index = positions.find(position => this.power(position) === this.exponent);
        if (index !== undefined) { this.input.setSelectionRange(index, index + 1); }
    }

    private help(): void {
        if (this.input.hasAttribute('aria-invalid')) { return; }
        const step = Math.pow(10, this.exponent);
        const scale = step >= 1e6 ? 1e6 : step >= 1e3 ? 1e3 : step >= 1 ? 1 : step >= .001 ? .001 : .000001;
        if (!this.options.frequency) {
            this.hint.textContent = this.dirty ? 'Enter applies · Esc cancels' : !this.tuning ? 'Click a digit · F2 to enter' :
                `Step ${step} ${this.options.unitLabel || ''} · ↑ ↓ or wheel`;
            return;
        }
        const unit = scale === 1e6 ? 'MHz' : scale === 1e3 ? 'kHz' : scale === 1 ? 'Hz' : scale === .001 ? 'mHz' : 'µHz';
        this.hint.textContent = this.dirty ? 'Enter applies · Esc cancels' : !this.tuning ? 'Click a digit · F2 to enter' :
            `Step ${Number((step / scale).toPrecision(8))} ${unit} · ↑ ↓ or wheel`;
    }

    setLimits(maximum: number, resolution: number): void {
        this.cancelQueued();
        this.options.maximum = maximum;
        this.options.resolution = resolution;
        this.minimumExponent = Math.ceil(Math.log(resolution) / Math.LN10);
        this.input.setAttribute('aria-valuemax', String(maximum - (this.options.inclusiveMaximum ? 0 : resolution)));
    }

    setValue(hz: number): void {
        this.accepted = hz;
        if (this.dirty || this.queued || this.inFlight) { return; }
        this.desired = hz;
        // Normalize harmless DDS rounding before formatting the fixed digit groups.
        for (let digits = 1; digits <= 15; digits++) {
            const candidate = Number(hz.toPrecision(digits));
            if (Math.abs(candidate - hz) <= this.options.resolution / 2) { this.desired = candidate; break; }
        }
        this.clearError();
        this.paint();
    }

    dispose(): void {
        this.disposed = true;
        this.cancelQueued();
        this.removers.forEach(remove => remove());
        this.releaseWheel();
    }
}

// Frequency controls share exactly the same editing and tuning behavior.
interface FrequencyInputOptions {
    value: number;
    maximum: number;
    resolution: number;
    inclusiveMaximum?: boolean;
    commit: (frequency: number) => Promise<number>;
    validation?: (message: string) => void;
}
class FrequencyInput extends DigitInput {
    constructor(input: HTMLInputElement, unit: HTMLSelectElement, options: FrequencyInputOptions) {
        super(input, unit, {...options, frequency: true, units: {Hz: 1, kHz: 1e3, MHz: 1e6, GHz: 1e9}});
    }
}
class NumberInput extends DigitInput {
    constructor(input: HTMLInputElement, options: DigitInputOptions) {
        const unit = input.ownerDocument.createElement('select');
        const option = input.ownerDocument.createElement('option');
        option.value = option.textContent = options.unitLabel || '';
        unit.appendChild(option);
        super(input, unit, {...options, inclusiveMaximum: true});
    }
}
