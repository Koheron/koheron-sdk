// Plot widget
// (c) Koheron

class Plot {

    private range_x: jquery.flot.range;

    constructor(document: Document, private driver, private plotBasics: PlotBasics) {
        // this.rangeFunction = "setTimeRange";
        this.updatePlot();
        this.range_x = <jquery.flot.range>{
            "from": 0,
            "to": 1024
        };
    }

    updatePlot() {
        this.driver.getFifoBuffer( (ch0: number[][], ch1: number[][]) => {
            this.plotBasics.redrawTwoChannels(ch0, ch1, this.range_x, "Channel 1", "Channel 2", true, true, () => {
                requestAnimationFrame( () => {
                    this.updatePlot();
                });
            });
        });
    }

}