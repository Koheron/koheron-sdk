// Control widget
// (c) Koheron

class Control {

    private width: number;
    private widthEdit: HTMLLinkElement;
    private widthInput: HTMLInputElement;
    private widthSave: HTMLLinkElement;
    private period: number;
    private periodEdit: HTMLLinkElement;
    private periodInput: HTMLInputElement;
    private periodSave: HTMLLinkElement;
    private countSpan: HTMLSpanElement;

    constructor(document: Document, private driver: PulseGenerator) {

        this.widthEdit = <HTMLLinkElement>document.getElementById('width-edit');
        this.widthInput = <HTMLInputElement>document.getElementById('width-input');
        this.widthSave = <HTMLLinkElement>document.getElementById('width-save');
        this.periodEdit = <HTMLLinkElement>document.getElementById('period-edit');
        this.periodInput = <HTMLInputElement>document.getElementById('period-input');
        this.periodSave = <HTMLLinkElement>document.getElementById('period-save');
        this.update();
    }

    update() {
        this.driver.getStatus( (status) => {
            this.widthEdit.innerHTML = status.width.toString();
            this.periodEdit.innerHTML = status.period.toString();
            requestAnimationFrame( () => { this.update(); } )
        });
    }

    editWidth() {
        this.widthEdit.style.display = 'none';
        this.widthInput.style.display = 'inline';
        this.widthSave.style.display = 'inline';
        this.widthInput.value = this.widthEdit.innerHTML;
    }

    saveWidth() {
        this.widthEdit.style.display = 'inline';
        this.widthInput.style.display = 'none';
        this.widthSave.style.display = 'none';
        let width = parseInt(this.widthInput.value);
        this.driver.setPulseWidth(width);
    }

    saveWidthKey(event: KeyboardEvent) {
        if (event.keyCode == 13) {
            this.saveWidth();
        }
    }

    editPeriod() {
        this.periodEdit.style.display = 'none';
        this.periodInput.style.display = 'inline';
        this.periodSave.style.display = 'inline';
        this.periodInput.value = this.periodEdit.innerHTML;
    }

    savePeriod() {
        this.periodEdit.style.display = 'inline';
        this.periodInput.style.display = 'none';
        this.periodSave.style.display = 'none';
        let period = parseInt(this.periodInput.value);
        this.driver.setPulsePeriod(period);
    }

    savePeriodKey(event: KeyboardEvent) {
        if (event.keyCode == 13) {
            this.savePeriod();
        }
    }

}