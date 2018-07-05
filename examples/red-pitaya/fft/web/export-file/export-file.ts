// Export file widget
// (c) Koheron

class ExportFile {

    private exportDataButtons: HTMLButtonElement[];
    private exportPlotButtons: HTMLButtonElement[];

    constructor(document: Document, private fft: FFT, private plot_:Plot) {
        this.exportDataButtons = <HTMLButtonElement[]><any>document.getElementsByClassName("export-data");
        this.exportPlotButtons = <HTMLButtonElement[]><any>document.getElementsByClassName("export-plot");
        this.initExportData();
        this.initExportPlot();
    }

    initExportData(): void {
        for (let i = 0; i < this.exportDataButtons.length; i++) {
            this.exportDataButtons[i].addEventListener('click', (event) => {

                let csvContent = "data:text/csv;charset=utf-8,";

                csvContent += "Koheron FFT \n";

                let dateTime = new Date();
                let fftWindowIndex: string = (<HTMLInputElement>document.querySelector("[data-command='setFFTWindow']")).value;
                csvContent += dateTime.getDate() + "/" + (dateTime.getMonth()+1)  + "/"  + dateTime.getFullYear() + " " ;
                csvContent += dateTime.getHours() + ":" + dateTime.getMinutes() + ":" + dateTime.getSeconds() + "\n\n";
                csvContent += '"Window",' + fftWindowIndex + "\n";
                csvContent += '"Input channel",' + (this.fft.status.channel).toString() + "\n";
                csvContent += '"Channel 0 DDS frequency (MHz)",' + (this.fft.status.dds_freq[0] / 1e6).toString() + "\n";
                csvContent += '"Channel 1 DDS frequency (MHz)",' + (this.fft.status.dds_freq[1] / 1e6).toString() + "\n\n\n";

                let yUnit: string = (<HTMLInputElement>document.querySelector(".unit-input:checked")).value;
                csvContent += '"Frequency (MHz)","Power spectral density (' + yUnit.replace("-", "/") + ')" \n';

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
                let canvas = this.plot_.plot.getCanvas();
                let imagePng = canvas.toDataURL("image/png").replace("image/png", "image/octet-stream");
                let exportGroup = this.exportPlotButtons[i].parentElement;
                let exportLink = <HTMLAnchorElement>(exportGroup.getElementsByTagName("a")[0]);
                exportLink.href = imagePng;
                exportLink.click();
            })
        }
    }

}
