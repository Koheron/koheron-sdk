// Export file widget
// (c) Koheron

class ExportFile {

    private exportDataButtons: HTMLButtonElement[];
    private exportPlotButtons: HTMLButtonElement[];

    constructor(document: Document, private plot_) {
        this.exportDataButtons = <HTMLButtonElement[]><any>document.getElementsByClassName("export-data");
        this.exportPlotButtons = <HTMLButtonElement[]><any>document.getElementsByClassName("export-plot");
        this.initExportData();
        this.initExportPlot();
    }

    valueToInputRange(inrange_value: number): string {
        if (inrange_value % 2 == 0) {
            return "2 V";
        } else {
            return "8 V";
        }
    }

    valueToInputChannel(inchan_value: number): string {
        if (inchan_value <= 1) {
            return inchan_value.toString();
        } else if (inchan_value == 2) { 
            return "0-1"
        } else {
            return "0+1";
        }
    }

    initExportData(): void {
        for (let i = 0; i < this.exportDataButtons.length; i++) {
            this.exportDataButtons[i].addEventListener('click', (event) => {
                let csvContent = "data:text/csv;charset=utf-8,";
                let dateTime = new Date();
                let referenceClock: string = (<HTMLInputElement>document.querySelector("[data-command='setReferenceClock']:checked")).dataset.valuestr;

                let fftWindowSelect = <HTMLSelectElement>document.querySelector("[data-command='setFFTWindow']");
                let fftWindowIndex: string = fftWindowSelect.options[fftWindowSelect.selectedIndex].innerHTML;

                let inputChannel: number = parseInt((<HTMLInputElement>document.querySelector("[name='input-channel']:checked")).value);
                let inputRangeCh0: number = parseInt((<HTMLInputElement>document.querySelector("[name='input-range-ch0']:checked")).value);
                let inputRangeCh1: number = parseInt((<HTMLInputElement>document.querySelector("[name='input-range-ch1']:checked")).value);

                csvContent += "Koheron ALPHA15 - Signal analyzer\n";
                csvContent += dateTime.getDate() + "/" + (dateTime.getMonth() + 1)  + "/"  + dateTime.getFullYear() + " " ;
                csvContent += dateTime.getHours() + ":" + dateTime.getMinutes() + ":" + dateTime.getSeconds() + "\n";
                csvContent += "\n";
                csvContent += '"Window",' + fftWindowIndex + "\n";
                csvContent += '"Input channel",' + this.valueToInputChannel(inputChannel) + "\n";
                csvContent += '"Input range channel 0",' + this.valueToInputRange(inputRangeCh0) + "\n";
                csvContent += '"Input range channel 1",' + this.valueToInputRange(inputRangeCh1) + "\n";
                csvContent += '"Reference clock (10 MHz)",' + referenceClock + "\n";

                csvContent += "\n\n";

                let yUnit: string = (<HTMLInputElement>document.querySelector(".unit-input:checked")).value;
                csvContent += '"Frequency (Hz)","' + this.plot_.yLabel + ' (' + yUnit.replace("-", "/") + ')" \n';

                this.plot_.plot_data.forEach( (rowArray) => {
                    let row = rowArray.join(",");
                    csvContent += row + "\n";
                });

                let exportGroup = this.exportDataButtons[i].parentElement;
                let exportLink = <HTMLAnchorElement>(exportGroup.getElementsByTagName("a")[0]);
                exportLink.href = encodeURI(csvContent);
                exportLink.click();
            });
        }
    }

    initExportPlot(): void {
        for (let i = 0; i < this.exportPlotButtons.length; i++) {
            this.exportPlotButtons[i].addEventListener('click', async (event) => {
                let canvas = this.plot_.plotBasics.plot.getCanvas();
                let imagePng = canvas.toDataURL("image/png").replace("image/png", "image/octet-stream");
                let exportGroup = this.exportPlotButtons[i].parentElement;
                let exportLink = <HTMLAnchorElement>(exportGroup.getElementsByTagName("a")[0]);
                exportLink.href = imagePng;
                exportLink.click();
            });
        }
    }
}
