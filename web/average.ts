// Average
// (c) Koheron

interface AverageStatus {
    isAverage: boolean;
    numAverageMin: number;
    numAverage: number;
}

class Average {

    private averageElements: any;
    private averageSpan: HTMLSpanElement;
    private numAverageMinInput: HTMLInputElement;
    private isAverage: boolean = false;

    constructor(document: Document, private driver: any) {

        this.averageElements = document.getElementsByClassName("average");
        this.averageSpan = <HTMLSpanElement>document.getElementById('avg');
        this.numAverageMinInput = <HTMLInputElement>document.getElementById('avg-min-input');

        this.update();
    }

    update() {
        this.driver.getAverageStatus((status: AverageStatus) => {
            this.isAverage = status.isAverage;
            if (status.isAverage) {
                for (let i: number = 0; i < this.averageElements.length; i++) {
                    this.averageElements[i].style.display = 'table-cell';
                }
                this.averageSpan.innerHTML = status.numAverage.toString();
            } else {
                for (let i: number = 0; i < this.averageElements.length; i++) {
                    this.averageElements[i].style.display = 'none';
                }
            }

            requestAnimationFrame( () => { this.update(); } )
        });
    }

    average(): void {
        this.driver.setAverage(!this.isAverage);
    }

    setNumAverageMin(numAverageMin) : void {
        if (this.isAverage) {
            this.driver.setNumAverageMin(Math.max(0, numAverageMin));
        }
    }

    saveNumAverageMin(): void {
        if (this.isAverage) {
            this.driver.setNumAverageMin(Math.max(0, parseInt(this.numAverageMinInput.value)));
        }
        this.numAverageMinInput.style.display = 'none';
    }

}