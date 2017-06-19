// Average
// (c) Koheron

interface AverageStatus {
    isAverage: boolean;
    numAverageMin: number;
    numAverage: number;
}

class Average {

    private averageBtn: HTMLLinkElement;
    private averageSpan: HTMLSpanElement;
    private numAverageMinInput: HTMLInputElement;
    private numAverageMinSave: HTMLLinkElement;
    private numAverageMinEdit: HTMLLinkElement;

    private isAverage: boolean = false;

    constructor(document: Document, private driver: any) {

        this.averageBtn = <HTMLLinkElement>document.getElementById('avg-btn');
        this.averageSpan = <HTMLSpanElement>document.getElementById('avg');
        this.numAverageMinInput = <HTMLInputElement>document.getElementById('avg-min-input');
        this.numAverageMinSave = <HTMLLinkElement>document.getElementById('avg-min-save');
        this.numAverageMinEdit = <HTMLLinkElement>document.getElementById('avg-min-edit');

        this.update();
    }

    update() {
        this.driver.getAverageStatus((status: AverageStatus) => {
            this.isAverage = status.isAverage;
            if (status.isAverage) {
                this.averageSpan.innerHTML = status.numAverage.toString();
                this.averageBtn.className = 'btn btn-primary-reversed active';
            } else {
                this.averageSpan.innerHTML = "";
                this.averageBtn.className = 'btn btn-primary-reversed';
            }
            requestAnimationFrame( () => { this.update(); } )
        });
    }

    average(): void {
        this.driver.setAverage(!this.isAverage);
    }

    editNumAverageMin(): void {
        this.numAverageMinEdit.style.display = 'none';
        this.numAverageMinInput.style.display = 'inline';
        this.numAverageMinSave.style.display = 'inline';
    }

    saveNumAverageMin(): void {
        this.numAverageMinEdit.innerHTML = this.numAverageMinInput.value;
        if (this.isAverage) {
            this.driver.setNumAverageMin(Math.max(0, parseInt(this.numAverageMinInput.value)));
        }
        this.numAverageMinEdit.style.display = 'inline';
        this.numAverageMinInput.style.display = 'none';
        this.numAverageMinSave.style.display = 'none';
    }

}