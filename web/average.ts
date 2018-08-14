// Average
// (c) Koheron

interface AverageStatus {
    isAverage: boolean;
    numAverageMin: number;
    numAverage: number;
}

class Average {

    private averageSwitch: HTMLInputElement;
    private averageMinInput: HTMLInputElement;
    private averageElements: any;
    private averageSpan: HTMLSpanElement;
    private isAverage: boolean = false;

    constructor(document: Document, private driver: any) {

        this.averageSwitch = <HTMLInputElement>document.getElementById("average-switch");
        this.initAverageSwitch();

        this.averageMinInput = <HTMLInputElement>document.getElementById("average-min-input");
        this.initAverageMinInput();

        this.averageElements = document.getElementsByClassName("average");
        this.averageSpan = <HTMLSpanElement>document.getElementById('average-value');

        this.update();
    }

    update() {
        this.driver.getAverageStatus((status: AverageStatus) => {
            this.isAverage = status.isAverage;
            if (status.isAverage) {
                this.averageSwitch.checked = true;
                for (let i: number = 0; i < this.averageElements.length; i++) {
                    this.averageElements[i].style.display = 'table-cell';
                }
                this.averageSpan.innerHTML = status.numAverage.toString();
            } else {
                this.averageSwitch.checked = false;
                for (let i: number = 0; i < this.averageElements.length; i++) {
                    this.averageElements[i].style.display = 'none';
                }
            }

            requestAnimationFrame( () => { this.update(); } )
        });
    }

    initAverageSwitch(): void {
        this.averageSwitch.addEventListener('change', (event) => {
            this.driver.setAverage(!this.isAverage);
        })
    }

    initAverageMinInput(): void {
        let events = ['change', 'input'];
        for (let event_ of events) {
            this.averageMinInput.addEventListener(event_, (event) => {
                if (this.isAverage) {
                    this.driver.setNumAverageMin(Math.max(0, parseInt((<HTMLInputElement>event.currentTarget).value)));
                }
            })
        }
    }
}