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

    initExportData(): void {
        for (let i = 0; i < this.exportDataButtons.length; i++) {
            this.exportDataButtons[i].addEventListener('click', (event) => {

                let csvContent = "data:text/csv;charset=utf-8,";
                let dateTime = new Date();
                let referenceClock: string = (<HTMLInputElement>document.querySelector("[data-command='setReferenceClock']:checked")).dataset.valuestr;
                let inputChannel: string = (<HTMLInputElement>document.querySelector("[name='channel']:checked")).value;
                let ddsInputs = <HTMLInputElement[]><any>document.querySelectorAll(".dds-input");
                let decimationRate: string = (<HTMLInputElement>document.querySelector(".cic-rate-input")).value;
                let nAverages: string = (<HTMLInputElement>document.querySelector(".plot-navg-input")).value;

                csvContent += "Koheron ALPHA250 \n";
                csvContent += "Phase Noise Analyzer \n";
                csvContent += dateTime.getDate() + "/" + (dateTime.getMonth()+1)  + "/"  + dateTime.getFullYear() + " " ;
                csvContent += dateTime.getHours() + ":" + dateTime.getMinutes() + ":" + dateTime.getSeconds() + "\n";
                csvContent += "\n";
                csvContent += '"Input channel",' + inputChannel + "\n";
                csvContent += '"Reference clock (10 MHz)",' + referenceClock + "\n";
                for (let i: number = 0; i < ddsInputs.length; i++) {
                    const units = {Hz: 1e-6, kHz: 1e-3, MHz: 1, GHz: 1e3};
                    const unit = (ddsInputs[i].parentElement.querySelector('.lo-unit') as HTMLSelectElement).value;
                    const frequencyMHz = Number(ddsInputs[i].value.replace(/\s/g, '')) * units[unit];
                    csvContent += '"LO ' + i + ' frequency (MHz)",' + frequencyMHz + "\n";
                }
                csvContent += '"Decimation rate",' + decimationRate + "\n";
                csvContent += '"Averages",' + nAverages + "\n";

                csvContent += "\n\n";
                csvContent += '"CARRIER OFFSET FREQUENCY (Hz)","' + this.plot_.yLabel +
                    '","' + this.plot_.yLabel + ' (smoothed)","PHASE PSD (rad^2/Hz)"\n';

                this.plot_.plot_data.forEach( (rowArray, index) => {
                    let row = rowArray.join(",") + "," + this.plot_.smooth_plot_data[index][1] +
                        "," + this.plot_.phase_psd[index];
                    csvContent += row + "\n";
                });

                let exportGroup = this.exportDataButtons[i].parentElement;
                let exportLink = <HTMLAnchorElement>(exportGroup.getElementsByTagName("a")[0]);
                exportLink.href = encodeURI(csvContent);
                exportLink.click();
            })
        }
    }

    initExportPlot(): void {
        for (let i = 0; i < this.exportPlotButtons.length; i++) {
            this.exportPlotButtons[i].addEventListener('click', (event) => {
                let canvas = this.plot_.plotBasics.plot.getCanvas();
                let imagePng = canvas.toDataURL("image/png").replace("image/png", "image/octet-stream");
                let exportGroup = this.exportPlotButtons[i].parentElement;
                let exportLink = <HTMLAnchorElement>(exportGroup.getElementsByTagName("a")[0]);
                exportLink.href = imagePng;
                exportLink.click();
            })
        }
    }

}
