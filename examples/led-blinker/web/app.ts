class App {
    private driver: LedBlinker;
    public control: Control;

    constructor(window: Window, document: Document, ip: string) {
        let client = new Client(ip, 5);

        window.addEventListener('load', () => {
            client.init( () => {
                this.driver = new LedBlinker(client);
                this.control = new Control(document, this.driver);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname);