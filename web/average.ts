// Average
// (c) Koheron

interface AverageStatus {
    avgOn: boolean;
    nMinAvg: number;
    nAvg: number;
}

class Average {

    private avgBtn: HTMLLinkElement;
    private avgSpan: HTMLSpanElement;
    private avgMinInput: HTMLInputElement;
    private avgMinSave: HTMLLinkElement;
    private avgMinEdit: HTMLLinkElement;

    private avgOn: boolean = false;

    constructor(document: Document, private driver: any) {

        this.avgBtn = <HTMLLinkElement>document.getElementById('avg-btn');
        this.avgSpan = <HTMLSpanElement>document.getElementById('avg');
        this.avgMinInput = <HTMLInputElement>document.getElementById('avg-min-input');
        this.avgMinSave = <HTMLLinkElement>document.getElementById('avg-min-save');
        this.avgMinEdit = <HTMLLinkElement>document.getElementById('avg-min-edit');

        this.update();
    }

    update() {
        this.driver.getAverageStatus((status: AverageStatus) => {
            this.avgOn = status.avgOn;
            if (status.avgOn) {
                this.avgSpan.innerHTML = status.nAvg.toString();
                this.avgBtn.className = 'btn btn-primary-reversed active';
            } else {
                this.avgSpan.innerHTML = "";
                this.avgBtn.className = 'btn btn-primary-reversed';
            }
            requestAnimationFrame( () => { this.update(); } )
        });
    }

    // Averaging

    avg(): void {
        this.driver.setAvg(!this.avgOn);
    }

    editAvgMin(): void {
        this.avgMinEdit.style.display = 'none';
        this.avgMinInput.style.display = 'inline';
        this.avgMinSave.style.display = 'inline';
    }

    saveAvgMin(): void {
        this.avgMinEdit.innerHTML = this.avgMinInput.value;
        if (this.avgOn) {
            this.driver.setNAvgMin(Math.max(0, parseInt(this.avgMinInput.value)));
        }
        this.avgMinEdit.style.display = 'inline';
        this.avgMinInput.style.display = 'none';
        this.avgMinSave.style.display = 'none';
    }

}