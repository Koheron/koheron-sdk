class InstrumentsWidget {

    private instrumentsTable: HTMLTableElement;
    private isUpdate: boolean;

    private uploadInput: HTMLInputElement;
    private uploadStatus: HTMLSpanElement;

    constructor (document: Document, private driver: Instruments) {
        this.instrumentsTable = <HTMLTableElement>document.getElementById('instruments-table');
        this.isUpdate = true;
        this.update();

        this.uploadInput = <HTMLInputElement>document.getElementById("upload-input");
        this.uploadStatus = <HTMLSpanElement>document.getElementById("upload-status");

    }

    update() {
        if (this.isUpdate) {

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
            });
        };

        setTimeout( () => {
            this.update(), 200
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

    uploadInstrumentClick() {
        this.uploadStatus.innerHTML = "";
        let file = this.uploadInput.files[0];
        if ((file.name).indexOf(".zip") !== -1) {
            this.driver.uploadInstrument(file, (status) => {
                this.isUpdate = status;
            });
        } else {
            this.uploadStatus.innerHTML = "Select ZIP file";
        }

    }

}
