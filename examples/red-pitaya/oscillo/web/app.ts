class App {

    private laserDriver: LaserDriver;
    private laserControl: LaserControl;
    private oscillo: Oscillo;
    private modulationDriver: ModulationDriver;
    private modulationControl: ModulationControl;
    private average: Average;
    private plot: Plot;
    private imports: Imports;
    private plotBasics: PlotBasics;

    wfmSize = 8192;
    samplingRate = 125e6;

    private n_pts: number;
    private x_min: number;
    private x_max: number;
    private y_min: number;
    private y_max: number;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let client = new Client(ip, 5);

        window.addEventListener('HTMLImportsLoaded', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.laserDriver = new LaserDriver(client);
                this.laserControl = new LaserControl(document, this.laserDriver);
                this.oscillo = new Oscillo(client);
                this.average = new Average(document, this.oscillo);

                this.modulationDriver = new ModulationDriver(client);
                this.modulationControl = new ModulationControl(document, this.modulationDriver, this.wfmSize, this.samplingRate);

                this.n_pts = 16384;
                this.x_min = 0;
                this.x_max = this.oscillo.maxT;
                this.y_min = -8192;
                this.y_max = +8191;

                this.plotBasics = new PlotBasics(document, plot_placeholder, this.n_pts, this.x_min, this.x_max, this.y_min, this.y_max, this.oscillo, "setTimeRange", "Time (µs)");
                this.plot = new Plot(document, this.oscillo, this.plotBasics);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };

    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));
