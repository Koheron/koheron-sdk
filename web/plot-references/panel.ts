class PlotReferencePanel<T extends PlotReference> {
    private disposed = false;
    private renderedItems: T[] = [];
    private undoIndex = 0;
    static mount(document: Document): void {
        const host = document.getElementById('plot-references');
        if (host && !document.getElementById('reference-list')) { host.innerHTML = `    <details class="reference-panel" open>
      <summary>References <span id="reference-count" class="reference-count">0 / 8</span></summary>
      <div class="reference-file-actions"><button id="save-references" type="button" disabled title="Download all references, including hidden traces, to reload in a later session">Save all</button><button id="load-references" type="button" title="Add references from a saved file">Load…</button><button id="clear-reference" type="button" disabled title="Remove all captured references">Clear all</button><input id="reference-file" type="file" accept=".json,application/json" hidden></div>
      <div id="reference-list"></div>
      <div id="reference-undo" class="reference-undo" hidden><span id="reference-undo-label" role="status"></span><button id="undo-reference" type="button">Undo</button></div>
      <p id="reference-message" role="status" aria-live="polite"></p>
    </details>`; }
    }
    constructor(private document: Document, private references: PlotReferences<T>, private redraw: () => void,
                private recapture: (item: T) => void, private describe: (item: T) => {text: string; title: string}) {
        PlotReferencePanel.mount(document);
        references.onChange = () => {
            const restoreFocus = this.rememberFocus();
            this.render(); redraw(); restoreFocus();
        };
        document.getElementById('save-references').addEventListener('click', () => {
            const url = URL.createObjectURL(new Blob([references.serialize()], {type:'application/json'}));
            const link = document.createElement('a'); link.href = url; link.download = 'koheron-' + references.board + '-references-' + new Date().toISOString().replace(/[:.]/g, '-') + '.json';
            link.click(); this.message('Download started · includes all ' + references.items.length + ' references.'); window.setTimeout(() => URL.revokeObjectURL(url), 1000);
        });
        document.getElementById('undo-reference').addEventListener('click', () => {
            references.undo(); this.message('Undo complete.');
        });
        const file = document.getElementById('reference-file') as HTMLInputElement;
        document.getElementById('load-references').addEventListener('click', () => file.click());
        file.addEventListener('change', async () => {
            const selected = file.files?.[0]; file.value = '';
            if (!selected) { return; }
            try {
                if (selected.size > 32 * 1024 * 1024) { throw new Error('Reference files must be smaller than 32 MB.'); }
                const text = await selected.text();
                if (this.disposed) { return; }
                const before = references.items.length;
                references.load(text);
                this.message('Added ' + (references.items.length - before) + ' references from ' + selected.name);
            } catch (error) { if (!this.disposed) { this.message(error instanceof Error ? error.message : 'Unable to load references.'); } }
        });
        this.render();
    }

    private message(text: string): void { this.document.getElementById('reference-message').textContent = text; }

    // Collection edits rebuild rows. Keep keyboard users at the same control,
    // or at its nearest remaining row; never steal focus from unrelated controls.
    private rememberFocus(): () => void {
        const d = this.document, list = d.getElementById('reference-list'), active = d.activeElement as HTMLElement;
        const row = active.closest('.reference-row');
        const index = Array.from(list.children).indexOf(row);
        const control = row ? Array.from(row.querySelectorAll('input, button')).indexOf(active) : -1;
        const item = this.renderedItems[index], undoIndex = this.undoIndex;
        const name = active instanceof d.defaultView.HTMLInputElement && active.type === 'text' ? active : undefined;
        const draft = name ? {value:name.value, start:name.selectionStart, end:name.selectionEnd, direction:name.selectionDirection} : undefined;
        return () => {
            let target: HTMLElement;
            if (index >= 0) {
                const kept = this.references.items.indexOf(item);
                const next = kept >= 0 ? kept : Math.min(index, list.children.length - 1);
                const nextRow = list.children[next];
                target = nextRow?.querySelectorAll<HTMLElement>('input, button')[control] || d.getElementById('undo-reference');
                if ((target as HTMLButtonElement).disabled) { target = nextRow.querySelector('input[type=text]'); }
                if (draft && kept >= 0) {
                    const input = target as HTMLInputElement;
                    input.value = draft.value; input.setSelectionRange(draft.start, draft.end, draft.direction);
                }
            } else if (active.id === 'undo-reference' && !this.references.undoLabel) {
                target = list.children[undoIndex]?.querySelector('input[type=text]');
            } else if (active.id === 'clear-reference' && !this.references.items.length) {
                target = d.getElementById('undo-reference');
            } else if (active.id === 'capture-reference' && (active as HTMLButtonElement).disabled) {
                target = list.lastElementChild?.querySelector('input[type=text]');
            }
            target?.focus();
        };
    }

    private render(): void {
        const d = this.document, list = d.getElementById('reference-list');
        if (this.references.undoLabel) {
            this.undoIndex = Math.max(0, this.renderedItems.findIndex((item, index) => this.references.items[index] !== item));
        }
        list.replaceChildren(); this.message('');
        d.getElementById('reference-count').textContent = this.references.items.length + ' / ' + PlotReferences.limit;
        d.getElementById('reference-undo').hidden = !this.references.undoLabel;
        d.getElementById('reference-undo-label').textContent = this.references.undoLabel;
        (d.getElementById('save-references') as HTMLButtonElement).disabled = !this.references.items.length;
        for (const item of this.references.items) {
            const row = d.createElement('div'); row.className = 'reference-row';
            const toggle = d.createElement('input'); toggle.type = 'checkbox'; toggle.checked = item.visible;
            toggle.setAttribute('aria-label', 'Show ' + item.name); toggle.style.accentColor = item.color;
            toggle.addEventListener('change', () => { item.visible = toggle.checked; this.redraw(); });
            const name = d.createElement('input'); name.type = 'text'; name.value = item.name; name.maxLength = 80;
            name.setAttribute('aria-label', 'Reference name'); name.style.borderLeft = '3px solid ' + item.color;
            const rename = () => {
                item.name = name.value.trim() || item.name; name.value = item.name;
                toggle.setAttribute('aria-label', 'Show ' + item.name); remove.setAttribute('aria-label', 'Remove ' + item.name); replace.setAttribute('aria-label', 'Recapture ' + item.name); this.redraw();
            };
            name.addEventListener('change', rename);
            name.addEventListener('keydown', event => {
                if (event.key === 'Enter') { event.preventDefault(); rename(); }
                if (event.key === 'Escape') { event.preventDefault(); name.value = item.name; }
            });
            const actions = d.createElement('div'); actions.className = 'reference-row-actions';
            const replace = d.createElement('button'); replace.type = 'button'; replace.className = 'replace-reference';
            replace.textContent = 'Recapture'; replace.title = 'Replace this capture with the displayed live spectrum';
            replace.setAttribute('aria-label', 'Recapture ' + item.name);
            replace.addEventListener('click', () => this.recapture(item));
            const remove = d.createElement('button'); remove.type = 'button'; remove.textContent = '×'; remove.className = 'remove-reference'; remove.title = 'Remove reference';
            remove.setAttribute('aria-label', 'Remove ' + item.name);
            remove.addEventListener('click', () => this.references.remove(item));
            const metadata = d.createElement('small');
            const description = this.describe(item);
            metadata.textContent = description.text; metadata.title = description.title;
            const time = d.createElement('time'); time.dateTime = item.capturedAt;
            const captured = new Date(item.capturedAt), today = new Date();
            time.textContent = captured.toDateString() === today.toDateString()
                ? captured.toLocaleTimeString([], {hour:'2-digit', minute:'2-digit', second:'2-digit'})
                : captured.toLocaleDateString([], {year:'numeric', month:'short', day:'numeric'});
            time.title = 'Captured ' + new Date(item.capturedAt).toLocaleString();
            actions.append(replace, remove); row.append(toggle, name, time, metadata, actions); list.append(row);
        }
        this.renderedItems = this.references.items.slice();
    }

    dispose(): void { this.disposed = true; this.references.onChange = () => {}; }
}
