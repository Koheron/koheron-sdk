class App {
    public control: Control;
    public plot: Plot;
    private driver: PulseGenerator;
    private imports: Imports;

    private plotBasics: PlotBasics;
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
                this.driver = new PulseGenerator(client);
                this.control = new Control(document, this.driver);

                this.n_pts = 1024;
                this.x_min = 0;
                this.x_max = 1024;
                this.y_min = -1.0;
                this.y_max = 1.0;

                this.plotBasics = new PlotBasics(document, plot_placeholder, this.plot, this.n_pts, this.x_min, this.x_max, this.y_min, this.y_max, this.driver, "", "FIFO Sample Number");
                this.plot = new Plot(document, this.driver, this.plotBasics);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));