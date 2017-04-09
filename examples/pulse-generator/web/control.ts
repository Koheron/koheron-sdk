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

        this.width = 128;
        this.widthEdit = <HTMLLinkElement>document.getElementById('width-edit');
        this.widthInput = <HTMLInputElement>document.getElementById('width-input');
        this.widthSave = <HTMLLinkElement>document.getElementById('width-save');
        this.period = 8192;
        this.periodEdit = <HTMLLinkElement>document.getElementById('period-edit');
        this.periodInput = <HTMLInputElement>document.getElementById('period-input');
        this.periodSave = <HTMLLinkElement>document.getElementById('period-save');
        this.countSpan = <HTMLSpanElement>document.getElementById('count');

        this.driver.setPulseGenerator(this.width, this.period);
        this.update();
    }

    update() {
        this.driver.getCount( (i) => {
            this.countSpan.innerHTML = i.toString();
            this.widthEdit.innerHTML = (this.width).toString();
            this.periodEdit.innerHTML = (this.period).toString();
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
        this.width = parseInt(this.widthInput.value);
        this.driver.setPulseGenerator(this.width, this.period);
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
        this.period = parseInt(this.periodInput.value);
        this.driver.setPulseGenerator(this.width, this.period);
    }

    savePeriodKey(event: KeyboardEvent) {
        if (event.keyCode == 13) {
            this.savePeriod();
        }
    }

}