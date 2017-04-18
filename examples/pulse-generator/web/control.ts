// Control widget
// (c) Koheron

class Control {

    private nPoints: number;
    private samplingFrequency: number;

    private width: number;
    private widthEdit: HTMLLinkElement;
    private widthInput: HTMLInputElement;
    private widthSave: HTMLLinkElement;
    private period: number;
    private periodEdit: HTMLLinkElement;
    private periodInput: HTMLInputElement;
    private periodSave: HTMLLinkElement;

    constructor(document: Document, private driver: PulseGenerator) {

        this.nPoints = 8192;
        this.samplingFrequency = 125e6;

        this.width = 128;
        this.widthEdit = <HTMLLinkElement>document.getElementById('width-edit');
        this.widthInput = <HTMLInputElement>document.getElementById('width-input');
        this.widthSave = <HTMLLinkElement>document.getElementById('width-save');
        this.period = 250000;
        this.periodEdit = <HTMLLinkElement>document.getElementById('period-edit');
        this.periodInput = <HTMLInputElement>document.getElementById('period-input');
        this.periodSave = <HTMLLinkElement>document.getElementById('period-save');

        this.driver.setPulseGenerator(this.width, this.period);
        this.generatePulses();
        this.update();
    }

    update() {
        this.widthEdit.innerHTML = (this.width).toString();
        this.periodEdit.innerHTML = (this.period).toString();
        requestAnimationFrame( () => { this.update(); } )
    }

    generatePulses() {

        let tRange: number[] = [];

        for (let i = 0; i < this.nPoints; i++) {
            tRange[i] = i / this.samplingFrequency;
        }

        let dacData0: number[] = [];
        let dacData1: number[] = [];

        dacData0 = new Array(this.nPoints);
        dacData1 = new Array(this.nPoints);

        for (let i=0; i < tRange.length ; i++ ) {
            dacData0[i] = 0.6 * Math.exp( - Math.pow((tRange[i] - (500 * Math.pow(10,-9))), 2) / Math.pow((150 * Math.pow(10,-9)),2) );
            dacData0[i] = ((Math.floor( this.nPoints * dacData0[i] ) + 8192) % (2 * this.nPoints)) + this.nPoints ;
            dacData1[i] = 0.6 * Math.exp( - Math.pow((tRange[i] - (500 * Math.pow(10,-9))), 2) / Math.pow((150 * Math.pow(10,-9)),2) );
            dacData1[i] = ((Math.floor( this.nPoints * dacData1[i] ) + 8192) % (2 * this.nPoints)) + this.nPoints;
        }

        let dacData: Uint32Array;

        dacData = new Uint32Array(this.nPoints);

        for (let i=0; i < tRange.length ; i++ ) {
            dacData[i] = dacData0[i] + 65536 * dacData1[i];
        }

        this.driver.setDacData(dacData);

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