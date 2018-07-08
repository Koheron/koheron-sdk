class App {

    public laserDriver: LaserDriver;
    public laserControl: LaserControl;
    public spectrum: Spectrum;
    public modulationDriver: ModulationDriver;
    public modulationControl: ModulationControl;
    public average: Average;
    public plot: Plot;
    private navigation: Navigation;
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

        window.addEventListener('load', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.laserDriver = new LaserDriver(client);
                this.laserControl = new LaserControl(document, this.laserDriver);
                this.spectrum = new Spectrum(client);
                this.average = new Average(document, this.spectrum);
                this.modulationDriver = new ModulationDriver(client);
                this.modulationControl = new ModulationControl(document, this.modulationDriver, this.wfmSize, this.samplingRate);

                this.n_pts = 8192;
                this.x_min = 0;
                this.x_max = 8192;
                this.y_min = 0;
                this.y_max = 200;

                this.plotBasics = new PlotBasics(document, plot_placeholder, this.plot, this.n_pts, this.x_min, this.x_max, this.y_min, this.y_max, this.spectrum, "setFreqRange");
                this.plot = new Plot(document, this.spectrum, this.plotBasics);
                this.navigation = new Navigation(document);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };

    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));