class ClockGeneratorApp {

    private clkgenInputs: HTMLInputElement[];

    constructor(document: Document, private driver) {
        this.clkgenInputs = <HTMLInputElement[]><any>document.getElementsByClassName("clkgen-input");
        this.initClkgenInputs();
        this.updateReferenceClock();
    }

    private initClkgenInputs(): void {
        for (let i = 0; i < this.clkgenInputs.length; i ++) {
            this.clkgenInputs[i].addEventListener('change', (event) => {
                this.driver[(<HTMLInputElement>event.currentTarget).dataset.command](parseInt((<HTMLInputElement>event.currentTarget).value));
            })
        }
    }

    private updateReferenceClock() {
        this.driver.getReferenceClock( (clkin: number) => {

            let clkIndex: string = "0";
            if (clkin !== 0) {
                clkIndex = "2";
            }

            (<HTMLInputElement>document.querySelector("[data-command='setReferenceClock'][value='" + clkIndex + "']")).checked = true;
            requestAnimationFrame( () => { this.updateReferenceClock(); } )
        });
    }

}