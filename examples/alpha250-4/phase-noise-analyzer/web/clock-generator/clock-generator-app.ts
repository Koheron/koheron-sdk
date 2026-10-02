class ClockGeneratorApp {
    private clkgenInputs: HTMLInputElement[];

    constructor(document: Document, private driver) {
        this.clkgenInputs = <HTMLInputElement[]><any>document.getElementsByClassName("clkgen-input");
        this.initClkgenInputs();
    }

    private initClkgenInputs(): void {
        for (let i = 0; i < this.clkgenInputs.length; i ++) {
            this.clkgenInputs[i].addEventListener('change', (event) => {
                this.driver[(<HTMLInputElement>event.currentTarget).dataset.command](parseInt((<HTMLInputElement>event.currentTarget).value));
            })
        }
    }
}