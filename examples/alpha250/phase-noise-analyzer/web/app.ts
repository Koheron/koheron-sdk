class App {

    private imports: Imports;
    private dds: DDS;
    public ddsFrequency: DDSFrequency;
    private clockGenerator: ClockGenerator;
    private clockGeneratorApp: ClockGeneratorApp;
    private phaseNoiseAnalyzer: PhaseNoiseAnalyzer;
    private phaseNoiseAnalyzerApp: PhaseNoiseAnalyzerApp;
    public plot: Plot;
    private plotBasics: PlotBasics;

    private n_pts: number;
    private x_min: number;
    private x_max: number;
    private y_min: number;
    private y_max: number;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let sockpoolSize: number = 10;
        let client = new Client(ip, sockpoolSize);

        window.addEventListener('HTMLImportsLoaded', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.dds = new DDS(client);
                this.ddsFrequency = new DDSFrequency(document, this.dds);
                this.clockGenerator = new ClockGenerator(client);
                this.clockGeneratorApp = new ClockGeneratorApp(document, this.clockGenerator);
                this.phaseNoiseAnalyzer = new PhaseNoiseAnalyzer(client);
                this.phaseNoiseAnalyzerApp = new PhaseNoiseAnalyzerApp(document, this.phaseNoiseAnalyzer);

                this.phaseNoiseAnalyzerApp.init( () => {
                    this.n_pts = this.phaseNoiseAnalyzerApp.nPoints;
                    this.x_min = 100;
                    this.x_max = 2E6;
                    this.y_min = -200;
                    this.y_max = 0;

                    this.plotBasics = new PlotBasics(document, plot_placeholder, this.plot, this.n_pts, this.x_min, this.x_max, this.y_min, this.y_max, this.phaseNoiseAnalyzer, "", "FREQUENCY OFFSET (Hz)");
                    this.plot = new Plot(document, this.phaseNoiseAnalyzer, this.plotBasics);
                })
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));