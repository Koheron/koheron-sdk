// Hosts keep their commands; this helper owns their DOM listener lifecycle.
class InstrumentEvents {
    private disposed = false;
    private remove: (() => void)[] = [];

    listen(target: EventTarget, type: string, handler: EventListener): void {
        if (this.disposed) { return; }
        const listener: EventListener = event => {
            if (!this.disposed) { handler(event); }
        };
        target.addEventListener(type, listener);
        this.remove.push(() => target.removeEventListener(type, listener));
    }

    dispose(): void {
        this.disposed = true;
        this.remove.forEach(remove => remove());
        this.remove = [];
    }
}
