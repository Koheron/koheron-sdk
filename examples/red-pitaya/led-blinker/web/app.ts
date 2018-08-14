class App {
    private driver: LedBlinker;
    public control: LedBlinkerControl;
    private imports: Imports;

    constructor(window: Window, document: Document, ip: string) {
        let client = new Client(ip, 5);

        window.addEventListener('HTMLImportsLoaded', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.driver = new LedBlinker(client);
                this.control = new LedBlinkerControl(document, this.driver, 8);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname);