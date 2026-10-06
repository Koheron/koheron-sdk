// Each loop owns its editors so polling never moves a draft between channels.
class Control {
  private frequencies: FrequencyInput[] = [];
  private gainRows: HTMLTableRowElement[];
  private removers: Array<() => void> = [];
  private disposed = false;
  private sampleRate = 0;
  private pendingGains = new Set<HTMLTableRowElement>();

  constructor(private document: Document, private dpll: Dpll,
              private fail: (error: unknown) => void) {
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
      this.listen(input, 'input', () => { save.disabled = false; input.setCustomValidity(''); });
      this.listen(input, 'keydown', event => {
        if ((event as KeyboardEvent).key === 'Enter') { save.click(); }
        if ((event as KeyboardEvent).key === 'Escape') {
          save.disabled = true;
          input.setCustomValidity('');
          void this.refreshGains();
        }
      });
      for (const button of Array.from(row.querySelectorAll<HTMLButtonElement>('.gain-button'))) {
        this.listen(button, 'click', () => {
          this.selectSign(row, Number(button.value));
          if (!input.value) { input.value = '0'; }
          save.disabled = false;
          input.setCustomValidity('');
        });
      }
      this.listen(save, 'click', async () => {
        if (this.pendingGains.has(row)) { return; }
        const sign = Number(row.querySelector<HTMLButtonElement>('[aria-pressed="true"]').value);
        // Zero needs no exponent; 0 * 2^NaN must never reach the integer RPC.
        const exponent = sign === 0 ? 0 : Number(input.value);
        const step = Math.round(exponent * 16);
        if (sign !== 0 && (!input.value || !input.checkValidity() || !Number.isFinite(exponent) ||
            exponent < 0 || exponent > 31 || (sign > 0 && step === 496))) {
          input.setCustomValidity('Use multiples of 1/16 from 0 to 30.9375 (31 for negative gains).');
          input.reportValidity();
          return;
        }
        this.pendingGains.add(row);
        save.disabled = true;
        try {
          await this.dpll.setGeometricGain(Number(row.dataset.channel), Number(row.dataset.gain), sign, step);
          this.pendingGains.delete(row);
          await this.refreshGains();
        } catch (error) { this.pendingGains.delete(row); this.fail(error); }
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
  }

  private listen(target: HTMLElement, event: string, listener: EventListener): void {
    target.addEventListener(event, listener);
    this.removers.push(() => target.removeEventListener(event, listener));
  }

  private selectSign(row: HTMLTableRowElement, sign: number): void {
    for (const button of Array.from(row.querySelectorAll<HTMLButtonElement>('.gain-button'))) {
      button.setAttribute('aria-pressed', String(Number(button.value) === sign));
    }
    row.querySelector<HTMLInputElement>('.gain-input').disabled = sign === 0;
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
      const applied = row.querySelector<HTMLOutputElement>('.gain-value');
      applied.value = String(gain);
      applied.title = `Applied gain: ${gain}`;
      if (!save.disabled || this.pendingGains.has(row)) { continue; }
      this.selectSign(row, Math.sign(gain));
      row.querySelector<HTMLInputElement>('.gain-input').value = gain === 0 ? '' :
        String(Math.round(Math.log(Math.abs(gain)) / Math.LN2 * 16) / 16);
    }
  }

  render(status: IDpllStatus, routes: number[], sampleRate: number): void {
    if (this.disposed) { return; }
    if (sampleRate !== this.sampleRate) {
      this.sampleRate = sampleRate;
      this.frequencies.forEach(frequency => frequency.setLimits(sampleRate / 2, sampleRate / Math.pow(2, 48)));
    }
    this.frequencies.forEach((frequency, channel) => frequency.setValue(status.dds_freq[channel]));
    this.renderGains(status);
    for (const input of Array.from(this.document.querySelectorAll<HTMLInputElement>('.integrator-switch'))) {
      input.checked = !!(status.integrators[Number(input.dataset.channel)] & (1 << Number(input.dataset.integratorindex)));
    }
    for (const select of Array.from(this.document.querySelectorAll<HTMLSelectElement>('.dac-output'))) {
      select.value = String(routes[Number(select.dataset.channel)]);
    }
  }

  dispose(): void {
    this.disposed = true;
    this.frequencies.forEach(frequency => frequency.dispose());
    this.removers.forEach(remove => remove());
  }
}
