// Shared capture collection. Instrument adapters own sample formats and validation.
interface PlotReference {
    name: string;
    capturedAt: string;
    visible: boolean;
    color: string;
}

abstract class PlotReferences<T extends PlotReference> {
    static readonly limit = 8;
    // Flot labels are HTML; names in controls and exports remain plain text.
    static traceLabel(name: string): string {
        return name.replace(/[&<>"']/g, char => ({'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;'}[char]));
    }
    protected colors = ['#a178b5', '#d55e00', '#0072b2', '#009e73', '#cc79a7', '#827000', '#555555', '#7a3e00'];
    items: T[] = [];
    private undoItems: T[];
    private replacedIndex = -1;
    undoLabel = '';
    onChange: () => void = () => {};
    constructor(readonly board: string) {}

    protected add(capture: Omit<T, keyof PlotReference>, capturedAt = new Date().toISOString()): void {
        if (this.items.length >= PlotReferences.limit) { throw new Error('Keep up to 8 references. Remove one before capturing another.'); }
        const used = new Set(this.items.map(item => item.color));
        let number = 1;
        while (this.items.some(item => item.name === 'Reference ' + number)) { number++; }
        this.forgetUndo();
        this.items.push({...capture, name: 'Reference ' + number, capturedAt, visible: true,
            color: this.colors.find(color => !used.has(color)) || this.colors[0]} as T);
        this.onChange();
    }

    protected replaceCapture(item: T, capture: Omit<T, keyof PlotReference>, capturedAt = new Date().toISOString()): void {
        const index = this.items.indexOf(item); if (index < 0) { return; }
        this.replacedIndex = index;
        this.undoItems = this.items.slice(); this.undoLabel = 'Recaptured ' + item.name;
        // Keep presentation edits, but discard all derived rendering caches.
        this.items[index] = {...capture, name:item.name, color:item.color, visible:item.visible, capturedAt} as T;
        this.onChange();
    }

    private forgetUndo(): void { this.undoItems = undefined; this.undoLabel = ''; this.replacedIndex = -1; }

    remove(item: T): void {
        if (!this.items.includes(item)) { return; }
        this.replacedIndex = -1;
        this.undoItems = this.items.slice(); this.undoLabel = 'Removed ' + item.name;
        this.items = this.items.filter(reference => reference !== item); this.onChange();
    }

    clear(): void {
        if (!this.items.length) { return; }
        this.replacedIndex = -1;
        this.undoItems = this.items.slice(); this.undoLabel = 'Cleared ' + this.items.length + ' references';
        this.items = []; this.onChange();
    }

    undo(): void {
        if (!this.undoItems) { return; }
        if (this.replacedIndex >= 0) {
            // Undo acquisition changes without discarding later naming/visibility edits.
            const current = this.items[this.replacedIndex], previous = this.undoItems[this.replacedIndex];
            previous.name = current.name; previous.visible = current.visible;
        }
        this.items = this.undoItems; this.forgetUndo(); this.onChange();
    }

    abstract serialize(): string;
    abstract load(text: string): void;

    protected parseFile(text: string, format: string): any[] {
        let file: any;
        try { file = JSON.parse(text); } catch (_) { throw new Error('Could not read this file. Choose a saved reference JSON file.'); }
        if (!file || file.format !== format || file.version !== 1) {
            throw new Error('Invalid reference file. Use a file saved by this instrument interface.');
        }
        if (file.board !== this.board) { throw new Error('This reference file is for a different board.'); }
        if (!Array.isArray(file.references) || !file.references.length || file.references.length > PlotReferences.limit) {
            throw new Error('Invalid reference collection.');
        }
        if (file.references.length + this.items.length > PlotReferences.limit) {
            throw new Error('Loading this file would exceed 8 references. Remove some references first.');
        }
        for (const item of file.references) {
            if (!item || typeof item.name !== 'string' || !item.name.trim() || item.name.length > 80 ||
                typeof item.capturedAt !== 'string' || !Number.isFinite(Date.parse(item.capturedAt)) ||
                typeof item.visible !== 'boolean' || typeof item.color !== 'string' || !/^#[0-9a-f]{6}$/i.test(item.color)) {
                throw new Error('Invalid reference metadata.');
            }
        }
        return file.references;
    }

    protected append(parsed: T[]): void {
        this.forgetUndo();
        for (const item of parsed) {
            const base = item.name; let number = 2;
            while (this.items.some(existing => existing.name === item.name)) {
                item.name = base.slice(0, 72) + ' (' + number++ + ')';
            }
            if (this.items.some(existing => existing.color === item.color)) {
                item.color = this.colors.find(color => !this.items.some(existing => existing.color === color)) || item.color;
            }
            this.items.push(item);
        }
        this.onChange();
    }
}
