
class App {
    private monitor: Monitor;
    private navigation: Navigation;

    private temperatureSpan: HTMLSpanElement;

    constructor(window: Window, document: Document, ip: string) {
        let client = new Client(ip, 5);
        this.temperatureSpan = <HTMLSpanElement>document.getElementById('temperature');

        window.addEventListener('load', () => {
            client.init( () => {
                this.monitor = new Monitor(client);
                this.navigation = new Navigation(document);
                this.update();
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

    update() {
        this.monitor.getTemperature((temperature: number) => {
            this.temperatureSpan.innerHTML = temperature.toFixed(3).toString();
            requestAnimationFrame( () => {
                this.update();
            });
        });
    }

}

let app = new App(window, document, location.hostname);