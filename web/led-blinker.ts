// LED Blinker
// (c) Koheron

class LedBlinker {
    private driver: Driver;
    private id: number;
    private cmds: HashTable<ICommand>;

    constructor (private client: Client) {
        this.driver = this.client.getDriver('LedBlinker');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getLeds(cb: (value: number) => void): void {
        this.client.readUint32(Command(this.id, this.cmds['get_leds']),
                                 (value) => {cb(value)});
    }

    setLed(index:number, status: boolean): void {
        this.client.send(Command(this.id, this.cmds['set_led'], index, status));
    }

}

class LedBlinkerControl {

    private ledBtns: HTMLInputElement[];
    private ledStatus: boolean[];

    constructor(private document: Document, private driver: LedBlinker, private ledCount: number) {
        this.ledBtns = [];
        this.ledBtns = <HTMLInputElement[]><any>document.getElementsByClassName("led-btn");
        this.ledStatus = [];
        this.initLedTable(this.ledCount, () => {
            this.initLedButtons();
            this.update();
        })
    }

    initLedTable(ledCount: number, callback): void {

        let ledTable = document.getElementById("leds-table");

        let tr0 = document.createElement("tr");
        let th0 = document.createElement("th");
        th0.textContent = "LEDs";
        tr0.appendChild(th0);
        for (let i = 0; i < ledCount; i++) {
            let th = document.createElement("th");
            th.textContent = i.toString();
            th.style.width = "100px";
            th.style.textAlign = "center";
            tr0.appendChild(th);
        }

        ledTable.appendChild(tr0);

        let tr1 = document.createElement("tr");
        let td0 = document.createElement("td");
        td0.textContent = "ON / OFF";
        tr1.appendChild(td0);
        for (let i = 0; i < ledCount; i++) {
            let td = document.createElement("td");
            td.style.width = "100 px";
            td.style.padding = "0px 5px";
            let input = document.createElement("input");
            input.type = "button";
            input.dataset.ledindex = i.toString();
            input.className = "led-btn";
            input.style.width = "100%";
            input.style.boxShadow = "0px 4px 6px 0px #e6e6e6";
            td.appendChild(input);
            tr1.appendChild(td);
        }

        ledTable.appendChild(tr1);
        callback();

    }

    initLedButtons(): void {
        for (let i = 0; i < this.ledBtns.length; i++) {
            this.ledBtns[i].addEventListener('click', (event) => {
                let index: number = parseInt((<HTMLInputElement>event.currentTarget).dataset.ledindex);
                console.log(index);
                this.driver.setLed(index, !this.ledStatus[index]);
            })
        }
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
}
