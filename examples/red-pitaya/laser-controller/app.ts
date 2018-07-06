class App {

    public laserControl: LaserControl;
    private laserDriver: LaserDriver;
    private navigation: Navigation;
    private imports: Imports;

    constructor(window: Window, document: Document, ip: string) {
        let client = new Client(ip, 5);

        window.addEventListener('load', () => {
            client.init( () => {
                this.imports = new Imports(document);
                this.laserDriver = new LaserDriver(client);
                this.laserControl = new LaserControl(document, this.laserDriver);
                this.navigation = new Navigation(document);
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }
}

let app = new App(window, document, location.hostname);