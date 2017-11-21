// LED Blinker
// (c) Koheron

class Control {

    private ledBtns: HTMLInputElement[];
    private ledStatus: boolean[];

    constructor(private document: Document, private driver: LedBlinker) {

        this.ledBtns = [];
        this.ledStatus = [];

        for (let i: number = 0; i < 8 ; i++) {
            this.ledBtns[i] = <HTMLInputElement>document.getElementById('led-' + i.toString());
        }

        this.update();

    }

    update(): void {
        this.driver.getLeds( (value) => {
            for (let i: number = 0; i < 8 ; i++) {
                this.ledStatus[i] = (((value >> i) % 2) === 1);
                if (this.ledStatus[i]) {
                    this.ledBtns[i].value = 'OFF';
                }
                else {
                    this.ledBtns[i].value = 'ON';
                }
            };
            requestAnimationFrame( () => { this.update(); } )
        });
    }

    switchLed(index: number) {
        this.driver.setLed(index, !this.ledStatus[index]);
    }

}
