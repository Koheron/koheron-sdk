// The schematic follows applied telemetry only. Its channel selector never writes hardware.
class DpllDiagram {
  private svg: SVGSVGElement;
  private latest: {status: IDpllStatus, routes: number[], sampleRate: number};
  private disposed = false;
  private originals = new Map<Element, string>();
  private channel: HTMLSelectElement;
  private removers: Array<() => void> = [];
  private editor: HTMLElement;
  private editedControl: HTMLElement;
  private placeholder: HTMLElement;
  private anchor: Element;
  private change = () => { this.closeEditor(false); this.highlight(null); this.paint(); };

  private closeEditor(restoreFocus: boolean): void {
    if (!this.editedControl) { return; }
    const frequency = this.editedControl.querySelector<HTMLInputElement>('.frequency-input');
    if (frequency && !this.disposed) { this.commitFrequency(Number(frequency.dataset.channel)); }
    if (!this.editedControl) { return; } // A failed commit can synchronously dispose the controls.
    // Blur before moving the node so digit tuning releases its wheel listener.
    if (this.editedControl.contains(this.document.activeElement)) { (this.document.activeElement as HTMLElement).blur(); }
    this.placeholder.replaceWith(this.editedControl);
    this.editedControl = null;
    this.editor.hidden = true;
    this.anchor.setAttribute('aria-expanded', 'false');
    if (restoreFocus) { (this.anchor as SVGElement).focus({preventScroll:true}); }
    this.highlight(restoreFocus ? this.controlFor(this.anchor) : null);
  }

  private updatePlaceholder(): void {
    if (!this.editedControl || !this.latest) { return; }
    const {status, routes} = this.latest;
    const channel = Number(this.channel.value);
    if (this.editedControl.matches('.gain-row')) {
      const gain = status[this.editedControl.dataset.status][channel];
      const db = this.editedControl.querySelector<HTMLInputElement>('.gain-input').dataset.unit === 'db';
      const cells = (this.placeholder as HTMLTableRowElement).cells;
      cells[1].textContent = gain === 0 ? '0' : gain < 0 ? '−' : '+';
      cells[2].textContent = gain === 0 ? 'Off' : db ? DpllGain.db(gain) : String(DpllGain.step(gain) / 16);
    } else if (this.editedControl.matches('.frequency-row')) {
      this.placeholder.textContent = `Reference · ${this.frequencyLabel(status.dds_freq[channel])}`;
    } else if (this.editedControl.matches('.integrator-row')) {
      this.placeholder.textContent = `Integrators · ${[0,1,2,3].filter(i => status.integrators[channel] & (1 << i)).map(i => i + 1).join(', ') || 'Off'}`;
    } else if (this.editedControl.matches('.p-mode-row')) {
      this.placeholder.textContent = `P + I · ${status.p_path[channel] & 1 ? 'Fast' : 'Accurate'}`;
    } else {
      this.placeholder.textContent = routes.map((route, dac) => `DAC ${dac} · ${this.routeName(route)}`).join(' / ');
    }
  }

  private frequencyLabel(hz: number): string {
    return `${Number((hz / 1e6).toFixed(6))} MHz`;
  }

  private routeName(route: number): string {
    return ['Loop 0', 'Loop 1', 'φ₀ LSB', 'φ₁ LSB', 'φ₀ MSB', 'φ₁ MSB', 'DDS 0', 'DDS 1'][route] || '?';
  }

  private positionEditor(): void {
    if (!this.editedControl) { return; }
    const box = this.anchor.getBoundingClientRect();
    const view = this.document.defaultView;
    const width = Math.min(280, view.innerWidth - 16);
    this.editor.style.width = `${width}px`;
    const height = this.editor.offsetHeight;
    this.editor.style.left = `${Math.max(8, Math.min(view.innerWidth - width - 8, box.left + box.width / 2 - width / 2))}px`;
    const below = box.bottom + 8;
    this.editor.style.top = `${Math.max(8, Math.min(view.innerHeight - height - 8, below + height > view.innerHeight - 8 ? box.top - height - 8 : below))}px`;
  }

  private listen(target: EventTarget, event: string, listener: EventListener): void {
    target.addEventListener(event, listener);
    this.removers.push(() => target.removeEventListener(event, listener));
  }

  private controlFor(block: Element): HTMLElement {
    const channel = this.channel.value;
    const gain = block.getAttribute('data-gain');
    if (gain) { return this.document.querySelector<HTMLElement>(`.gain-row[data-channel="${channel}"][data-status="${gain}"]`); }
    if (block.hasAttribute('data-integrator')) {
      return this.document.querySelector<HTMLElement>(`.integrator-switch[data-channel="${channel}"][data-integratorindex="${block.getAttribute('data-integrator')}"]`);
    }
    if (block.getAttribute('data-control') === 'dds') { return this.document.querySelector<HTMLElement>(`.frequency-input[data-channel="${channel}"]`); }
    return this.document.querySelector<HTMLElement>(block.getAttribute('data-control') === 'mode' ? `.p-mode[data-channel="${channel}"]` : '.routing-controls');
  }

  private highlight(control: Element): void {
    for (const element of Array.from(this.document.querySelectorAll('#live-diagram [data-linked], .gain-row[data-linked], .integrators label[data-linked], .p-mode-row[data-linked], .frequency-row[data-linked], .routing-controls[data-linked]'))) { element.removeAttribute('data-linked'); }
    if (!control || !this.svg) { return; }
    const row = control.closest('.gain-row');
    const input = control.closest('.integrator-switch') || control.closest('.integrators label')?.querySelector('.integrator-switch');
    const mode = control.closest('.p-mode-row')?.querySelector('.p-mode');
    const frequency = control.closest('.frequency-row')?.querySelector('.frequency-input');
    const routing = control.closest('.routing-controls');
    const source = row || input || mode || frequency;
    if (!routing && (!source || source.getAttribute('data-channel') !== this.channel.value)) { return; }
    (row || input?.parentElement || mode?.parentElement || frequency?.closest('.frequency-row') || routing).setAttribute('data-linked', 'true');
    const selector = row ? `[data-gain="${row.getAttribute('data-status')}"]` : input ? `[data-integrator="${input.getAttribute('data-integratorindex')}"]` : `[data-control="${mode ? 'mode' : frequency ? 'dds' : 'routing'}"]`;
    for (const block of Array.from(this.svg.querySelectorAll(selector))) { block.setAttribute('data-linked', 'true'); }
  }

  private edit(block: Element): void {
    if (this.disposed || !this.latest) { return; }
    if (this.anchor === block && this.editedControl) { this.closeEditor(true); return; }
    this.closeEditor(false);
    if (this.disposed) { return; }
    const target = this.controlFor(block);
    if (!target) { return; }
    const control = target.closest<HTMLElement>('.gain-row, .integrator-row, .p-mode-row, .frequency-row, .routing-controls');
    const gainInput = control.querySelector<HTMLInputElement>('.gain-input');
    this.anchor = block;
    this.editedControl = control;
    // Move the original controls: one draft, one set of listeners, one hardware action.
    if (gainInput) {
      const placeholder = this.document.createElement('tr');
      placeholder.className = 'gain-placeholder';
      placeholder.style.height = `${Math.max(24, control.getBoundingClientRect().height)}px`;
      for (let i = 0; i < 4; i++) { placeholder.insertCell(); }
      placeholder.cells[0].textContent = control.querySelector('label').textContent;
      this.placeholder = placeholder;
    } else {
      this.placeholder = this.document.createElement('div');
      this.placeholder.className = 'diagram-placeholder';
      const style = this.document.defaultView.getComputedStyle(control);
      this.placeholder.style.height = `${control.getBoundingClientRect().height}px`;
      this.placeholder.style.marginTop = style.marginTop;
      this.placeholder.style.marginBottom = style.marginBottom;
    }
    this.placeholder.title = 'Applied settings';
    this.placeholder.setAttribute('aria-hidden', 'true');
    this.updatePlaceholder();
    control.replaceWith(this.placeholder);
    const table = this.editor.querySelector<HTMLTableElement>('.gain-table');
    table.hidden = !gainInput;
    (gainInput ? table.querySelector('tbody') : this.editor.querySelector('.diagram-editor-controls')).appendChild(control);
    const name = gainInput ? control.querySelector('label').textContent : target.matches('.p-mode') ? 'P + I' : target.matches('.integrator-switch') ? 'Integrators' : target.matches('.frequency-input') ? 'Reference DDS' : 'RF DAC routing';
    this.document.getElementById('diagram-editor-title').textContent = control.matches('.routing-controls') ? name : `ADC ${this.channel.value} · ${name}`;
    this.document.getElementById('diagram-editor-unit').textContent = gainInput ? (gainInput.dataset.unit === 'db' ? 'dB' : 'log₂') : '';
    this.editor.hidden = false;
    block.setAttribute('aria-expanded', 'true');
    this.positionEditor();
    const focus = gainInput ? (gainInput.disabled ? control.querySelector<HTMLElement>('.gain-button[aria-pressed="true"]') : gainInput) : target.matches('input, select') ? target : target.querySelector<HTMLElement>('select');
    focus.focus({preventScroll:true});
    if (gainInput && !gainInput.disabled) { gainInput.select(); }
    this.highlight(focus);
  }

  constructor(private document: Document, private commitFrequency: (channel: number) => void) {
    this.channel = document.querySelector<HTMLSelectElement>('#diagram-channel');
    this.editor = document.getElementById('diagram-editor');
    this.listen(document.getElementById('diagram-editor-close'), 'click', () => this.closeEditor(true));
    this.listen(this.editor, 'keydown', event => {
      const key = event as KeyboardEvent;
      if (key.key === 'Tab') {
        const controls = Array.from(this.editor.querySelectorAll<HTMLElement>('button, input, select')).filter(control => !control.matches(':disabled') && control.getClientRects().length > 0);
        const boundary = key.shiftKey ? controls[0] : controls[controls.length - 1];
        // Let the browser continue its normal tab order from the originating block.
        if (event.target === boundary) { this.closeEditor(true); }
      }
      if (key.key === 'Escape') {
        event.preventDefault();
        const frequency = this.editedControl?.querySelector<HTMLInputElement>('.frequency-input');
        if (frequency && event.target !== frequency) { frequency.dispatchEvent(new KeyboardEvent('keydown', {key:'Escape'})); }
        if (this.editedControl && !this.editedControl.contains(event.target as Node)) {
          this.editedControl.dispatchEvent(new KeyboardEvent('keydown', {key:'Escape'}));
        }
        this.closeEditor(true);
      }
    });
    this.listen(document, 'pointerdown', event => {
      const target = event.target as Node;
      if (this.editedControl && !this.editor.contains(target) && !this.anchor.contains(target)) { this.closeEditor(false); }
    });
    this.listen(document, 'dpll-gain-applied', event => {
      if (this.editedControl === (event as CustomEvent).detail) { this.closeEditor(true); }
    });
    this.listen(document.defaultView, 'resize', () => this.positionEditor());
    this.listen(document, 'scroll', () => this.closeEditor(false));
    this.listen(document.querySelector('.diagram-panel details'), 'toggle', () => {
      if (!document.querySelector<HTMLDetailsElement>('.diagram-panel details').open) { this.closeEditor(false); }
    });
    this.listen(this.channel, 'change', this.change);
    this.listen(document, 'focusin', event => {
      const target = event.target as Element;
      if (this.editedControl && !this.editor.contains(target) && !this.svg?.contains(target)) { this.closeEditor(false); }
      if (target.closest('#diagram-editor')) { this.highlight(target); return; }
      if (!target.closest('.channel-panel')) {
        if (!this.svg?.contains(target)) { this.highlight(target); }
        return;
      }
      const source = target.closest('[data-channel]');
      if (source) {
        this.channel.value = source.getAttribute('data-channel');
        this.paint();
        this.highlight(target);
      }
    });
    this.listen(document, 'pointerover', event => {
      const target = event.target as Element;
      if (target.closest('.channel-panel, .routing-controls, #diagram-editor')) { this.highlight(target); }
    });
    this.listen(document, 'pointerout', event => {
      const target = event.target as Element;
      const related = (event as PointerEvent).relatedTarget as Node;
      const source = target.closest('.gain-row, .integrators label, .p-mode-row, .frequency-row, .routing-controls');
      if (source && (!related || !source.contains(related))) { this.highlight(this.document.activeElement); }
    });
    this.listen(document, 'focusout', event => {
      const next = (event as FocusEvent).relatedTarget as Element;
      this.highlight(next);
    });
    void this.load();
  }

  private async load(): Promise<void> {
    try {
      const response = await fetch('p_path.svg');
      if (!response.ok) { return; }
      const source = new DOMParser().parseFromString(await response.text(), 'image/svg+xml');
      if (this.disposed || source.querySelector('parsererror')) { return; }
      this.svg = this.document.importNode(source.documentElement, true) as unknown as SVGSVGElement;
      this.svg.setAttribute('role', 'group');
      for (const block of Array.from(this.svg.querySelectorAll('[data-gain], [data-integrator], [data-control]'))) {
        block.setAttribute('role', 'button');
        block.setAttribute('aria-haspopup', 'dialog');
        block.setAttribute('aria-controls', 'diagram-editor');
        block.setAttribute('aria-expanded', 'false');
        this.listen(block, 'click', () => this.edit(block));
        this.listen(block, 'keydown', event => {
          if (['Enter', ' '].indexOf((event as KeyboardEvent).key) >= 0) { event.preventDefault(); this.edit(block); }
        });
        this.listen(block, 'pointerover', () => this.highlight(this.controlFor(block)));
        this.listen(block, 'pointerout', () => this.highlight(this.document.activeElement));
        this.listen(block, 'focus', () => this.highlight(this.controlFor(block)));
      }
      for (const text of Array.from(this.svg.querySelectorAll('[data-label], [data-gain] text, [data-gain] title, [data-integrator] title, [data-control] title'))) {
        this.originals.set(text, text.textContent);
      }
      const host = this.document.querySelector('#live-diagram');
      host.replaceChild(this.svg, host.firstElementChild);
      this.paint();
      this.highlight(this.document.activeElement);
    } catch (_) {
      // Keep the static image if the optional live SVG cannot load.
    }
  }

  render(status: IDpllStatus, routes: number[], sampleRate: number): void {
    if (this.disposed) { return; }
    this.latest = {status, routes, sampleRate};
    this.updatePlaceholder();
    this.paint();
  }

  private text(element: Element, value: string): void {
    if (element.textContent !== value) { element.textContent = value; }
  }

  private paint(): void {
    if (!this.svg || !this.latest || this.disposed) { return; }
    const {status, routes, sampleRate} = this.latest;
    const channel = Number(this.channel.value);
    const fast = !!(status.p_path[channel] & 1);
    const label = (name: string, value: string) => this.text(this.svg.querySelector(`[data-label="${name}"]`), value);
    this.svg.setAttribute('data-live', 'true');
    for (const panel of Array.from(this.document.querySelectorAll('.channel-panel'))) {
      panel.setAttribute('data-diagram-selected', String(!!panel.querySelector(`[data-channel="${channel}"]`)));
    }
    label('header', `ADC${channel} · ${sampleRate / 1e6} MS/s · APPLIED SETTINGS`);
    label('adc', `ADC${channel}`);
    label('dds', this.frequencyLabel(status.dds_freq[channel]));
    label('mode', fast ? 'Fast selected' : 'Accurate selected');
    label('range', fast ? (status.p_path[channel] & 2 ? 'Within estimate range' : 'Outside estimate range') : 'Calibrated near lock');
    label('routes', routes.map((route, dac) => `DAC${dac}: ${this.routeName(route)}`).join(' · '));
    // Highlight only the selector inputs: Accurate continues to feed I², I³ and the monitor in Fast mode.
    for (const wire of Array.from(this.svg.querySelectorAll('[data-mode]'))) {
      wire.setAttribute('data-selected', String(wire.getAttribute('data-mode') === (fast ? 'fast' : 'accurate')));
    }
    for (const block of Array.from(this.svg.querySelectorAll('[data-gain]'))) {
      const gain = status[block.getAttribute('data-gain')][channel];
      const text = block.querySelector('text');
      const name = this.originals.get(text);
      block.setAttribute('data-state', gain === 0 ? 'disabled' : 'enabled');
      this.text(text, gain === 0 ? name : `${gain < 0 ? '−' : '+'} ${name}`);
      this.text(block.querySelector('.gain-db'), gain === 0 ? 'Off' : `${DpllGain.db(gain)} dB`);
      const description = `ADC ${channel} ${name}: ${DpllGain.description(gain)} (applied). Edit gain.`;
      this.text(block.querySelector('title'), description);
      block.setAttribute('aria-label', description);
      block.setAttribute('tabindex', '0');
    }
    for (const block of Array.from(this.svg.querySelectorAll('[data-control]'))) {
      const description = block.getAttribute('data-control') === 'dds' ? `ADC ${channel} reference DDS: ${Number(status.dds_freq[channel].toPrecision(15))} Hz (applied). Edit frequency.` : block.getAttribute('data-control') === 'mode' ? `ADC ${channel} P + I: ${fast ? 'Fast' : 'Accurate'}. Select mode.` : `${routes.map((route, dac) => `DAC ${dac}: ${this.routeName(route)}`).join('. ')}. Edit routing.`;
      this.text(block.querySelector('title'), description);
      block.setAttribute('aria-label', description);
      block.setAttribute('tabindex', '0');
    }
    for (const block of Array.from(this.svg.querySelectorAll('[data-integrator]'))) {
      const index = Number(block.getAttribute('data-integrator'));
      const enabled = !!(status.integrators[channel] & (1 << index));
      block.setAttribute('data-state', enabled ? 'enabled' : 'disabled');
      const description = `ADC ${channel} integrator ${index + 1}: ${enabled ? 'enabled' : 'disabled · state reset to zero'}. Edit integrators.`;
      this.text(block.querySelector('title'), description);
      block.setAttribute('aria-label', description);
      block.setAttribute('tabindex', '0');
    }
  }

  dispose(): void {
    this.disposed = true;
    this.closeEditor(false);
    this.removers.forEach(remove => remove());
    this.highlight(null);
    for (const panel of Array.from(this.document.querySelectorAll('[data-diagram-selected]'))) { panel.removeAttribute('data-diagram-selected'); }
    if (!this.svg) { return; }
    this.svg.removeAttribute('data-live');
    for (const block of Array.from(this.svg.querySelectorAll('[role="button"]'))) {
      block.removeAttribute('role'); block.removeAttribute('tabindex'); block.removeAttribute('aria-label');
      block.removeAttribute('aria-haspopup'); block.removeAttribute('aria-controls'); block.removeAttribute('aria-expanded');
    }
    this.originals.forEach((value, element) => this.text(element, value));
    this.text(this.svg.querySelector('[data-label="header"]'), 'READBACK UNAVAILABLE · SCHEMATIC');
  }
}
