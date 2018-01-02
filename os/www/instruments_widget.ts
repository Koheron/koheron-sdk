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

                this.instrumentsTable.innerHTML = '<thead><tr><th>Name</th><th>Status</th><th colspan="2">Action</th></tr></thead>';

                for (let instrument of instruments) {
                    let row = this.instrumentsTable.insertRow(-1);

                    let nameCell = row.insertCell(0);
                    let statusCell = row.insertCell(1);
                    let runCell = row.insertCell(2);

                    let isLive: boolean = false;

                    for (let cell of [nameCell, statusCell, runCell]) {
                        cell.style.border = 'none';
                        cell.style.verticalAlign = 'middle';
                    }

                    if (instrument == liveInstrument) {
                        isLive = true;
                    };

                    this.setStatusCell(statusCell, instrument, isLive);
                    this.setRunCell(runCell, instrument, isLive);
                    nameCell.innerHTML = instrument;

                }
                this.isUpdate = false;
            });
        };

        setTimeout( () => {
            this.update(), 200
        });
    }

    setStatusCell(cell: any, name: string, isLive: boolean): void {
        if (isLive === true) {
            cell.innerHTML = '<a href="/">Running</a>';
        } else {
            cell.innerHTML = '';
        }
    }

    setRunCell(cell: any, name: string, isLive: boolean): void {
        if (isLive === true) {
            cell.innerHTML = '';
        } else {
            cell.innerHTML = '<a onclick="instruments_widget.runClick(this.parentNode, \'' + name + '\'); return false;" href="#">Run</a>';
        }
    }

    runClick (cell: any, name: string): void {
        document.body.style.cursor = "wait";
        this.driver.runInstrument(name, (err) => {
            document.body.style.cursor = "default";
            location.href = "/";
        })
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
