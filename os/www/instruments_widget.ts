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

                this.instrumentsTable.innerHTML = '<thead><tr><th>Name</th><th>Status</th><th colspan="2">Action</th><th>Version</th></tr></thead>';

                for (let instrument of instruments) {

                    let instrumentObj = {};

                    if (typeof(instrument) === "string") {
                        instrumentObj["name"] = instrument;
                        instrumentObj["version"] = "";
                    } else {
                        instrumentObj = instrument;
                    }

                    let liveInstrumentObj = {};

                    if (typeof(instrument) === "string") {
                        liveInstrumentObj["name"] = liveInstrument;
                        liveInstrumentObj["version"] = "";
                    } else {
                        liveInstrumentObj = liveInstrument;
                    }

                    let row = this.instrumentsTable.insertRow(-1);

                    let nameCell = row.insertCell(0);
                    let statusCell = row.insertCell(1);
                    let runCell = row.insertCell(2);
                    let deleteCell = row.insertCell(3);
                    let versionCell = row.insertCell(4);

                    let isLive: boolean = false;

                    for (let cell of [nameCell, statusCell, runCell, deleteCell, versionCell]) {
                        cell.style["border-left"] = 'none';
                        cell.style["border-right"] = 'none';
                        cell.style["border-top"] = "1px solid #ddd";
                        cell.style["border-bottom"] = "none";
                        cell.style.verticalAlign = 'middle';
                    }

                    if (instrumentObj["name"] == liveInstrumentObj["name"]) {
                        isLive = true;
                    };

                    this.setStatusCell(statusCell, isLive);
                    this.setRunCell(runCell, instrumentObj["name"], isLive);
                    this.setDeleteCell(deleteCell, instrumentObj["name"], isLive)
                    nameCell.innerHTML = instrumentObj["name"];
                    versionCell.innerHTML = instrumentObj["version"];

                }
                this.isUpdate = false;
                document.body.style.cursor = "default";

            });
        };

        setTimeout( () => {
            this.update(), 200
        });
    }

    setStatusCell(cell: any, isLive: boolean): void {
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

    setDeleteCell(cell: any, name: string, isLive: boolean): void {
        if (isLive === true) {
            cell.innerHTML = '';
        } else {
            cell.innerHTML = '<a onclick="instruments_widget.deleteClick(this.parentNode, \'' + name + '\'); return false;" href="#">Remove</a>';
        }
    }

    deleteClick (cell: any, name: string): void {
        document.body.style.cursor = "wait";
        this.driver.deleteInstrument(name);
        this.isUpdate = true;
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
