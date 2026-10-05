class RedPitayaFFTControls implements FFTBoardControls {
    constructor(private document: Document) {}
    async init(): Promise<void> {
        const note = this.document.getElementById('board-acquisition-note');
        note.textContent = '125 MS/s · 14 bit';
        note.hidden = false;
    }
    dispose(): void {}
}
