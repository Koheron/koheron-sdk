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
                let fftWindowIndex: string = (<HTMLInputElement>document.querySelector("[data-command='setFFTWindow']")).value;
                let inputChannel: string = (<HTMLInputElement>document.querySelector("[name='input-channel']:checked")).value;
                let samplingFrequency: string = (<HTMLInputElement>document.querySelector("[name='sampling-frequency']:checked")).dataset.valuestr;
                let ddsInputs = <HTMLInputElement[]><any>document.querySelectorAll(".dds-channel-input[type='range']");

                csvContent += "Koheron Alpha \n";
                csvContent += dateTime.getDate() + "/" + (dateTime.getMonth()+1)  + "/"  + dateTime.getFullYear() + " " ;
                csvContent += dateTime.getHours() + ":" + dateTime.getMinutes() + ":" + dateTime.getSeconds() + "\n";
                csvContent += "\n";
                csvContent += '"Window",' + fftWindowIndex + "\n";
                csvContent += '"Input channel",' + inputChannel + "\n";
                csvContent += '"Sampling frequency (MHz)",' + samplingFrequency + "\n";
                csvContent += '"Reference clock (10 MHz)",' + referenceClock + "\n";
                for (let i: number = 0; i < ddsInputs.length; i++) {
                    let channel: string = ddsInputs[i].dataset.channel;
                    csvContent += '"Channel ' + channel + ' DDS frequency (MHz)",' + ddsInputs[i].value + "\n";
                }

                csvContent += "\n\n";

                let yUnit: string = (<HTMLInputElement>document.querySelector(".unit-input:checked")).value;
                csvContent += '"Frequency (MHz)","' + this.plot_.yLabel + ' (' + yUnit.replace("-", "/") + ')" \n';

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
