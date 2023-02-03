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
                let ddsInputs = <HTMLInputElement[]><any>document.querySelectorAll(".dds-channel-input[type='range']");
                let decimationRate: string = (<HTMLInputElement>document.querySelector("[class='cic-rate-input']")).value;
                let nAverages: string = (<HTMLInputElement>document.querySelector("[class='plot-navg-input']")).value;

                csvContent += "Koheron ALPHA250 \n";
                csvContent += "Phase Noise Analyzer \n";
                csvContent += dateTime.getDate() + "/" + (dateTime.getMonth()+1)  + "/"  + dateTime.getFullYear() + " " ;
                csvContent += dateTime.getHours() + ":" + dateTime.getMinutes() + ":" + dateTime.getSeconds() + "\n";
                csvContent += "\n";
                csvContent += '"Input channel",' + inputChannel + "\n";
                csvContent += '"Reference clock (10 MHz)",' + referenceClock + "\n";
                for (let i: number = 0; i < ddsInputs.length; i++) {
                    let channel: string = ddsInputs[i].dataset.channel;
                    csvContent += '"Channel ' + channel + ' DDS frequency (MHz)",' + ddsInputs[i].value + "\n";
                }
                csvContent += '"Decimation rate",' + decimationRate + "\n";
                csvContent += '"Averages",' + nAverages + "\n";

                csvContent += "\n\n";
                csvContent += '"CARRIER OFFSET FREQUENCY (Hz)","' + this.plot_.yLabel + '"\n';

                this.plot_.plot_data.forEach( (rowArray) => {
                    let row = rowArray.join(",");
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
