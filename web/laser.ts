// Interface for the Laser driver
// (c) Koheron

interface LaserStatus {
    laser_on: boolean;
    current: number;
    measured_current: number;
    measured_power: number;
}

class LaserDriver {
    private driver: Driver;
    private id: number;
    private cmds: Commands;

    constructor(public client: Client) {
        this.driver = this.client.getDriver('Laser');
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    getStatus(cb: (status: LaserStatus) => void): void {
        this.client.readTuple(Command(this.id, this.cmds['get_status']), '?fff',
                             (tup: [boolean, number, number, number]) => {
            let status: LaserStatus = <LaserStatus>{};
            status.laser_on = tup[0];
            status.current = tup[1];
            status.measured_current = tup[2];
            status.measured_power = tup[3]
            cb(status);
        });
    }

    start(): void {
        this.client.send(Command(this.id, this.cmds['start']));
    }

    stop(): void {
        this.client.send(Command(this.id, this.cmds['stop']));
    }

    setCurrent(val: number): void {
        this.client.send(Command(this.id, this.cmds['set_current'], val));
    }

}

class LaserControl {
    private laserSwitch: HTMLInputElement;
    private inputCurrentSlider: HTMLInputElement;
    private inputCurrentSpan: HTMLSpanElement;
    private outputCurrentSpan: HTMLSpanElement;
    private outputPowerSpan: HTMLSpanElement;

    constructor(private document: Document, private driver: LaserDriver) {
        this.laserSwitch = <HTMLInputElement>document.getElementById('laser-switch');
        this.inputCurrentSlider = <HTMLInputElement>document.getElementById('input-current-slider');
        this.inputCurrentSpan = <HTMLSpanElement>document.getElementById('input-current');
        this.outputCurrentSpan = <HTMLSpanElement>document.getElementById('output-current');
        this.outputPowerSpan = <HTMLSpanElement>document.getElementById('output-power');

        this.update();
    }

    update(): void {
        this.driver.getStatus ( (status) => {
            this.outputPowerSpan.innerHTML = status.measured_power.toString();
            this.outputCurrentSpan.innerHTML = (status.measured_current * 1e3).toFixed(2).toString();

            this.inputCurrentSlider.value = status.current.toFixed(2).toString();
            this.inputCurrentSpan.innerHTML = status.current.toFixed(2).toString();

            if (status.laser_on) {
                this.laserSwitch.value = 'Stop Laser';
                this.laserSwitch.className = 'btn btn-danger';
            } else {
                this.laserSwitch.value = 'Start Laser';
                this.laserSwitch.className = 'btn btn-success';
            }
            requestAnimationFrame( () => { this.update(); });
        });
    }

    switchLaser(): void {
        if (this.laserSwitch.value == 'Start Laser') {
            this.driver.start();
        } else { // Turn off
            this.driver.stop();
        }
    }

    setCurrent(value: string): void {
        this.driver.setCurrent(parseFloat(value));
    }

}