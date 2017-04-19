class InstrumentsWidget {

    private instrumentsTable: HTMLTableElement;
    private isUpdate: boolean;

    constructor (document: Document, private driver: Instruments) {
        this.instrumentsTable = <HTMLTableElement>document.getElementById('instruments-table');
        this.isUpdate = true;
        this.update();
    }

    update() {
        if (this.isUpdate || this.driver.isNewInstrument) {

            this.driver.getInstrumentsStatus( (status) => {
                let instruments = status['instruments'];
                let liveInstrument = status['live_instrument'];

                this.instrumentsTable.innerHTML = '<thead><tr><th>Name</th><th>Action</th></tr></thead>';

                for (let instrument of instruments) {
                    let row = this.instrumentsTable.insertRow(-1);

                    let nameCell = row.insertCell(0);
                    let viewCell = row.insertCell(1);
                    let runCell = row.insertCell(1);

                    for (let cell of [nameCell, viewCell, runCell]) {
                        cell.style.border = 'none';
                        cell.style.verticalAlign = 'middle';
                    }

                    if (instrument == liveInstrument) {
                        this.setViewCell(runCell, instrument);
                    }
                    else {
                        this.setRunCell(runCell, instrument);
                    }

                    nameCell.innerHTML = instrument;
                }
                this.isUpdate = false;
                this.driver.isNewInstrument = false;
            });
        };

        setTimeout( () => {
            this.update(), 500
        });
    }

    setRunCell(cell: any, name: string): void {
        cell.innerHTML = '<a class="btn btn-default" onclick="instruments_widget.runClick(this.parentNode, \'' + name + '\'); return false;" href="#">Start</a>';
    }

    runClick (cell: any, name: string): void {
        this.isUpdate = false;
        this.driver.runInstrument(name, (err) => {
            this.setViewCell(cell, name);
            this.isUpdate = true;
        })
    }

    setViewCell(cell: any, name: string): void {
        cell.innerHTML = '<a class="btn btn-primary" href="/">View</a>';
    }

}
