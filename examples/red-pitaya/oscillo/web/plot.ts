// Plot
// (c) Koheron

class Plot {

    private ch1Checkbox: HTMLInputElement;
    private ch2Checkbox: HTMLInputElement;
    private rangeFunction: string;

    constructor(document: Document, private driver, private plotBasics: PlotBasics) {
        this.ch1Checkbox = <HTMLInputElement>document.getElementById('ch1-checkbox');
        this.ch2Checkbox = <HTMLInputElement>document.getElementById('ch2-checkbox');
        this.rangeFunction = "setTimeRange";
        this.updatePlot();
    }

    updatePlot() {
        let is_channel_1: boolean = this.ch1Checkbox.checked;
        let is_channel_2: boolean = this.ch2Checkbox.checked;
        this.driver.getDecimatedData( (ch0: number[][], ch1: number[][], range_x: jquery.flot.range) => {
            this.plotBasics.redrawTwoChannels(ch0, ch1, range_x, "Channel 1", "Channel 2", is_channel_1, is_channel_2, () => {
                requestAnimationFrame( () => {
                    this.updatePlot();
                });
            });
        });
    }

}