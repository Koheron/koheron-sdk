class App {
    public control: Control;
    private driver: DualDDS;
    private navigation: Navigation;
    private imports: Imports;
    private ddsFrequency: DDSFrequency;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let client = new Client(ip, 5);

        window.addEventListener('HTMLImportsLoaded', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.driver = new DualDDS(client);
                this.control = new Control(document, this.driver);
                this.navigation = new Navigation(document);
                this.ddsFrequency = new DDSFrequency(document, this.driver);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));