class App {
    public control: Control;
    public plot: Plot;
    private fft: FFT;
    public laserDriver: LaserDriver;
    public laserControl: LaserControl;

    constructor(window: Window, document: Document,
                ip: string, plot_placeholder: JQuery) {
        let client = new Client(ip, 5);

        window.addEventListener('load', () => {
            client.init( () => {
                this.fft = new FFT(client);

                this.fft.init( () => {
                    this.control = new Control(document, this.fft);
                    this.plot = new Plot(document, plot_placeholder, this.fft, this.control);
                    this.laserDriver = new LaserDriver(client);
                    this.laserControl = new LaserControl(document, this.laserDriver);
                });
            });
        }, false);

        window.onbeforeunload = () => { client.exit(); };
    }

}

let app = new App(window, document, location.hostname, $('#plot-placeholder'));