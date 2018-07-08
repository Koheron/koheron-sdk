// Plot
// (c) Koheron

class Plot {

    private xlabel: string;
    private ylabel: string;
    private xLabelDiv: HTMLDivElement;

    private velocitySwitch: HTMLInputElement;
    private isPlotVelocity: boolean;
    private velocity: Array<number>;

    private peakDatapoint: number[];

    constructor(document: Document, private driver, private plotBasics: PlotBasics) {
        this.ylabel = "Power Spectral Density (dB)";
        this.xLabelDiv = <HTMLDivElement>document.getElementById("x-label");
        this.xlabel = "Frequency (MHz)";
        this.isPlotVelocity = false;
        this.velocity = [];
        this.velocitySwitch = <HTMLInputElement>document.getElementById("velocity-switch");
        this.initVelocitySwitch();
        this.updatePlot();
    }

    updatePlot(): void {

        this.xLabelDiv.innerHTML = this.xlabel;

        if (this.isPlotVelocity) {
            this.driver.getPeakFifoData( (peakFifoData) => {

                var time: number[] = [];
                var plot_data: number[][] = [];

                const samplingFrequency: number = 125e6; //Hz
                const fftSize: number = 4096;
                const dopplerShift: number = 1.29e6;
                const n: number = Math.floor(samplingFrequency / fftSize);
                let fifoLength: number = peakFifoData.length;
                this.velocity = this.rollArray(this.velocity, fifoLength);

                for (let i: number = 0; i < fifoLength; i ++) {
                    this.velocity[n-i] = peakFifoData[fifoLength - i] * (samplingFrequency / fftSize) / dopplerShift;
                }

                for (let i : number = n; i > 0 ; i --) {
                    time[i] = (i / samplingFrequency) * fftSize;
                }
                for (let i: number = 0; i < n; i ++) {
                    plot_data[i] = [time[i], this.velocity[i]];
                }

                this.driver.setAddressRange(2, fftSize / 2);
                const range_x = {
                    from: 0,
                    to: 1
                }

                this.plotBasics.redrawRange(plot_data, range_x, this.ylabel, () => {
                    requestAnimationFrame( () => { this.updatePlot(); });
                });

            });
        } else {
            this.driver.getDecimatedData( (plot_data: number[][], range_x: jquery.flot.range) => {
                this.plotBasics.redrawRange(plot_data, range_x, this.ylabel, () => {
                    requestAnimationFrame( () => { this.updatePlot(); });
                });

            });
        }

    }

    initVelocitySwitch(): void {
        this.velocitySwitch.addEventListener('change', (event) => {
            if (this.isPlotVelocity) {
                this.ylabel = "Power Spectral Density (dB)";
                this.xlabel = "Frequency (MHz)";
                this.isPlotVelocity = false;
                // this.isResetRange = true;
            } else {
                this.ylabel = "Speed (m/s)";
                this.xlabel = "Time (s)";
                this.isPlotVelocity = true;
                // this.isResetRange = true;
            }
        })
    }

    rollArray(array, count) {
        // numpy roll
        count -= array.length * Math.floor(count / array.length);
        array.push.apply(array, array.splice(0, count))
        return array
    }

}