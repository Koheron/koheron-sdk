class App {

    public laserControl: LaserControl;
    private laserDriver: LaserDriver;

    constructor(window: Window, document: Document, ip: string) {
        let client = new Client(ip, 5);

        window.addEventListener('load', () => {
            client.init( () => {
                this.laserDriver = new LaserDriver(client);
                this.laserControl = new LaserControl(document, this.laserDriver);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname);