class App {
    public control: Control;
    public plot: Plot;
    private driver: PulseGenerator;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let client = new Client(ip, 5);

        window.addEventListener('load', () => {
            client.init( () => {
                this.driver = new PulseGenerator(client);
                this.control = new Control(document, this.driver);
                this.plot = new Plot(document, plot_placeholder, this.driver);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));