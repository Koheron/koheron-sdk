class App {

    private imports: Imports;
    private dds: DDS;
    public ddsFrequency: DDSFrequency;
    private clockGenerator: ClockGenerator;
    private clockGeneratorApp: ClockGeneratorApp;

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
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));