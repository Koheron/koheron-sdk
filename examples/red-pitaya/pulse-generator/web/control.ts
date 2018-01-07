// Control widget
// (c) Koheron

class Control {

    private nPoints: number;
    private samplingFrequency: number;

    private width: number;
    private widthInput: HTMLInputElement;
    private period: number;
    private periodInput: HTMLInputElement;

    constructor(document: Document, private driver: PulseGenerator) {

        this.nPoints = 8192;
        this.samplingFrequency = 125e6;

        this.width = 128;
        this.widthInput = <HTMLInputElement>document.getElementById('width-input');
        this.period = 250000;
        this.periodInput = <HTMLInputElement>document.getElementById('period-input');

        this.driver.setPulseWidth(this.width);
        this.driver.setPulsePeriod(this.period);

        this.generatePulses();
        this.update();
    }

    update() {
        this.driver.getStatus( (status) => {

            if (document.activeElement !== this.widthInput) {
                this.widthInput.value = status.width.toString();
            }

            if (document.activeElement !== this.periodInput) {
                this.periodInput.value = status.period.toString();
            }

            requestAnimationFrame( () => { this.update(); } )
        });
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

    setWidth(event) {;
        this.driver.setPulseWidth(event.value);
    }

    setPeriod(event) {
        this.driver.setPulsePeriod(event.value);
    }

}