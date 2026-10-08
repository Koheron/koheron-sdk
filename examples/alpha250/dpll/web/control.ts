// Each loop owns its editors so polling never moves a draft between channels.
class Control {
  private diagram: DpllDiagram;
  private frequencies: FrequencyInput[] = [];
  private gainRows: HTMLTableRowElement[];
  private removers: Array<() => void> = [];
  private disposed = false;
  private sampleRate = 0;
  private pendingGains = new Set<HTMLTableRowElement>();
  private gainSteps = new Map<HTMLTableRowElement, number>();
  private pendingPaths = new Set<HTMLSelectElement>();
  private pathErrors = new Map<number, string>();

  constructor(private document: Document, private dpll: Dpll,
              private fail: (error: unknown) => void) {
    this.diagram = new DpllDiagram(document, channel => this.frequencies[channel].commit());
    for (const input of Array.from(document.querySelectorAll<HTMLInputElement>('.frequency-input'))) {
      const channel = Number(input.dataset.channel);
      const unit = document.querySelector<HTMLSelectElement>(`.frequency-unit[data-channel="${channel}"]`);
      this.frequencies[channel] = new FrequencyInput(input, unit, {
        value: 0, maximum: 0, resolution: 1, inclusiveMaximum: true,
        commit: async frequency => {
          try {
            this.dpll.setDDSFreq(channel, frequency);
            const status = await this.dpll.getControlParameters();
            return status.dds_freq[channel];
          } catch (error) { this.fail(error); throw error; }
        }
      });
    }
    this.gainRows = Array.from(document.querySelectorAll<HTMLTableRowElement>('.gain-row'));
    for (const row of this.gainRows) {
      const input = row.querySelector<HTMLInputElement>('.gain-input');
      const save = row.querySelector<HTMLButtonElement>('.gain-save');
      this.configureGainInput(row);
      this.listen(input, 'input', () => { save.disabled = false; input.setCustomValidity(''); });
      this.listen(input, 'keydown', event => {
        const key = event as KeyboardEvent;
        if (key.key === 'Enter') { event.preventDefault(); save.click(); }
        if ((key.key === 'ArrowUp' || key.key === 'ArrowDown') && !key.ctrlKey && !key.metaKey && !key.altKey) {
          event.preventDefault();
          if (!input.value || !Number.isFinite(Number(input.value))) { return; }
          const sign = Number(row.querySelector<HTMLButtonElement>('[aria-pressed="true"]').value);
          const step = DpllGain.inputStep(input.value, input.dataset.unit) + (key.key === 'ArrowUp' ? 1 : -1) * (key.shiftKey ? 16 : 1);
          input.value = DpllGain.value(Math.max(0, Math.min(sign > 0 ? 495 : 496, step)), input.dataset.unit);
          input.setCustomValidity('');
          save.disabled = false;
        }
      });
      this.listen(row, 'keydown', event => {
        if ((event as KeyboardEvent).key === 'Escape') {
          event.preventDefault();
          save.disabled = true;
          input.setCustomValidity('');
          void this.refreshGains();
        }
      });
      for (const button of Array.from(row.querySelectorAll<HTMLButtonElement>('.gain-button'))) {
        this.listen(button, 'click', () => {
          this.selectSign(row, Number(button.value));
          if (!input.value && !input.disabled) { input.value = '0'; }
          save.disabled = false;
          input.setCustomValidity('');
        });
      }
      this.listen(save, 'click', async () => {
        if (this.pendingGains.has(row)) { return; }
        const sign = Number(row.querySelector<HTMLButtonElement>('[aria-pressed="true"]').value);
        // Zero needs no exponent; 0 * 2^NaN must never reach the integer RPC.
        const value = sign === 0 ? 0 : Number(input.value);
        const step = sign === 0 ? 0 : DpllGain.inputStep(input.value, input.dataset.unit);
        if (sign !== 0 && (!input.value || !input.checkValidity() || !Number.isFinite(value) ||
            step < 0 || step > (sign > 0 ? 495 : 496))) {
          input.setCustomValidity(input.dataset.unit === 'db' ? 'Use 0 to 186.26 dB (186.64 dB for negative gains); rounded to the nearest hardware step.' : 'Use multiples of 1/16 from 0 to 30.9375 (31 for negative gains).');
          input.reportValidity();
          return;
        }
        this.pendingGains.add(row);
        save.disabled = true;
        try {
          await this.dpll.setGeometricGain(Number(row.dataset.channel), Number(row.dataset.gain), sign, step);
          this.pendingGains.delete(row);
          await this.refreshGains();
          if (!this.disposed) { this.document.dispatchEvent(new CustomEvent('dpll-gain-applied', {detail: row})); }
        } catch (error) { this.pendingGains.delete(row); this.fail(error); }
      });
    }
    for (const select of Array.from(document.querySelectorAll<HTMLSelectElement>('.gain-unit'))) {
      this.listen(select, 'change', () => {
        const rows = this.gainRows.filter(row => row.dataset.channel === select.dataset.channel);
        const invalid = rows.map(row => row.querySelector<HTMLInputElement>('.gain-input')).find(input => !input.disabled && !input.checkValidity());
        if (invalid) {
          select.value = invalid.dataset.unit;
          invalid.reportValidity();
          return;
        }
        for (const row of rows) {
          const input = row.querySelector<HTMLInputElement>('.gain-input');
          const step = input.value ? DpllGain.inputStep(input.value, input.dataset.unit) : Number(input.dataset.resumeStep || 0);
          input.dataset.unit = select.value;
          this.configureGainInput(row);
          if (input.value) { input.value = DpllGain.value(step, select.value); }
        }
      });
    }
    for (const input of Array.from(document.querySelectorAll<HTMLInputElement>('.integrator-switch'))) {
      this.listen(input, 'change', () => {
        try { this.dpll.setIntegrator(Number(input.dataset.channel), Number(input.dataset.integratorindex), input.checked); }
        catch (error) { this.fail(error); }
      });
    }
    for (const select of Array.from(document.querySelectorAll<HTMLSelectElement>('.dac-output'))) {
      this.listen(select, 'change', () => {
        try { this.dpll.setDacOutput(Number(select.dataset.channel), Number(select.value)); }
        catch (error) { this.fail(error); }
      });
    }
    for (const select of Array.from(document.querySelectorAll<HTMLSelectElement>('.p-mode'))) {
      this.listen(select, 'change', async () => {
        if (this.pendingPaths.has(select)) { return; }
        const channel = Number(select.dataset.channel);
        this.pathErrors.delete(channel);
        this.pendingPaths.add(select); select.disabled = true;
        try {
          await this.dpll.setPMode(channel, Number(select.value));
          this.pendingPaths.delete(select);
          this.renderPaths(await this.dpll.getControlParameters());
        } catch (error) {
          this.pendingPaths.delete(select);
          if (error instanceof PModeError && error.code === -3) {
            this.pathErrors.set(channel, error.message);
            try { this.renderPaths(await this.dpll.getControlParameters()); }
            catch (readError) { this.fail(readError); }
          } else this.fail(error);
        }
      });
    }
  }

  private listen(target: HTMLElement, event: string, listener: EventListener): void {
    target.addEventListener(event, listener);
    this.removers.push(() => target.removeEventListener(event, listener));
  }

  private selectSign(row: HTMLTableRowElement, sign: number): void {
    for (const button of Array.from(row.querySelectorAll<HTMLButtonElement>('.gain-button'))) {
      button.setAttribute('aria-pressed', String(Number(button.value) === sign));
    }
    const input = row.querySelector<HTMLInputElement>('.gain-input');
    if (sign === 0) {
      if (input.value && Number.isFinite(Number(input.value))) {
        input.dataset.resumeStep = String(DpllGain.inputStep(input.value, input.dataset.unit));
      }
      input.value = '';
    } else if (!input.value && input.disabled) {
      input.value = DpllGain.value(Number(input.dataset.resumeStep || 0), input.dataset.unit);
    }
    input.placeholder = sign === 0 ? 'Off' : '';
    input.disabled = sign === 0;
    this.configureGainInput(row);
  }

  private configureGainInput(row: HTMLTableRowElement): void {
    const input = row.querySelector<HTMLInputElement>('.gain-input');
    const db = input.dataset.unit === 'db';
    const positive = row.querySelector<HTMLButtonElement>('[aria-pressed="true"]').value === '1';
    input.max = DpllGain.value(positive ? 495 : 496, input.dataset.unit);
    input.step = db ? 'any' : '0.0625';
    input.setAttribute('aria-label', `ADC ${row.dataset.channel} ${row.querySelector('label').textContent} gain ${db ? 'magnitude in dB' : 'exponent'}`);
    input.title = `${db ? '20 log₁₀ |g|, relative to coefficient 1. Rounded to ≈0.376 dB steps.' : 'Base-2 exponent, 16 steps per octave.'} ↑/↓: one step · Shift+↑/↓: one octave · Enter: apply · Escape: cancel`;
  }

  private async refreshGains(): Promise<void> {
    try { this.renderGains(await this.dpll.getControlParameters()); }
    catch (error) { this.fail(error); }
  }

  private renderGains(status: IDpllStatus): void {
    if (this.disposed) { return; }
    for (const row of this.gainRows) {
      const save = row.querySelector<HTMLButtonElement>('.gain-save');
      const gain = status[row.dataset.status][Number(row.dataset.channel)];
      if (gain !== 0) {
        this.gainSteps.set(row, DpllGain.step(gain));
      }
      if (!save.disabled || this.pendingGains.has(row)) { continue; }
      const input = row.querySelector<HTMLInputElement>('.gain-input');
      input.value = input.dataset.unit === 'db' && gain !== 0 ? DpllGain.db(gain) : DpllGain.value(this.gainSteps.get(row) || 0, input.dataset.unit);
      this.selectSign(row, Math.sign(gain));
    }
  }

  render(status: IDpllStatus, routes: number[], sampleRate: number): void {
    if (this.disposed) { return; }
    this.diagram.render(status, routes, sampleRate);
    if (sampleRate !== this.sampleRate) {
      this.sampleRate = sampleRate;
      this.frequencies.forEach(frequency => frequency.setLimits(sampleRate / 2, sampleRate / Math.pow(2, 48)));
    }
    this.frequencies.forEach((frequency, channel) => frequency.setValue(status.dds_freq[channel]));
    this.renderGains(status);
    this.renderPaths(status);
    for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('.integrator-switch'))) {
      input.checked = !!(status.integrators[Number(input.dataset.channel)] & (1 << Number(input.dataset.integratorindex)));
    }
    for (const select of Array.from(this.document.querySelectorAll<HTMLSelectElement>('.dac-output'))) {
      select.value = String(routes[Number(select.dataset.channel)]);
    }
  }

  private renderPaths(status: IDpllStatus): void {
    if (this.disposed) { return; }
    for (const select of Array.from(this.document.querySelectorAll<HTMLSelectElement>('.p-mode'))) {
      if (this.pendingPaths.has(select)) { continue; }
      const channel = Number(select.dataset.channel);
      const bits = status.p_path[channel];
      select.value = String(bits & 1); select.disabled = false;
      const output = this.document.querySelector<HTMLOutputElement>(`.p-mode-status[data-channel="${channel}"]`);
      output.textContent = this.pathErrors.get(channel) || (bits & 1 ?
        (bits & 2 ? 'Fast · within estimate range' : 'Fast · outside estimate range') :
        'Accurate · full phase range');
      output.title = output.textContent;
    }
  }

  dispose(): void {
    this.disposed = true;
    this.frequencies.forEach(frequency => frequency.dispose());
    this.diagram.dispose();
    this.removers.forEach(remove => remove());
  }
}
