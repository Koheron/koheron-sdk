class App {
    public control: Control;
    private driver: DualDDS;
    private navigation: Navigation;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let client = new Client(ip, 5);

        window.addEventListener('load', () => {
            client.init( () => {
                this.driver = new DualDDS(client);
                this.control = new Control(document, this.driver);
                this.navigation = new Navigation(document);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));