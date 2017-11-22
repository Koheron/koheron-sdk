// Interface for the Laser driver
// (c) Koheron

interface LaserStatus {
    laser_on: boolean;
    constant_power_on: boolean;
    is_calibrated: boolean;
    current: number;
    power: number;
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
        this.client.readTuple(Command(this.id, this.cmds['get_status']), '???ffff',
                             (tup: [boolean, boolean, boolean, number, number, number, number]) => {
            let status: LaserStatus = <LaserStatus>{};
            status.laser_on = tup[0];
            status.constant_power_on = tup[1];
            status.is_calibrated = tup[2];
            status.current = tup[3];
            status.power = tup[4];
            status.measured_current = tup[5];
            status.measured_power = tup[6];
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

    setPower(val: number): void {
        this.client.send(Command(this.id, this.cmds['set_power'], val));
    }

    switchMode(): void {
        this.client.send(Command(this.id, this.cmds['switch_mode']));
    }

    calibrate0mW(): void {
        this.client.send(Command(this.id, this.cmds['calibrate_0mW']));
    }

    calibrate1mW(): void {
        this.client.send(Command(this.id, this.cmds['calibrate_1mW']));
    }

}

class LaserControl {
    private laserSwitch: HTMLLinkElement;
    private modeSwitch: HTMLLinkElement;
    private calibrationSpan: HTMLSpanElement;
    private currentControlTr: any;
    private inputCurrentSlider: HTMLInputElement;
    private inputCurrentSpan: HTMLSpanElement;
    private powerControlTr: any;
    private inputPowerSlider: HTMLInputElement;
    private inputPowerSpan: HTMLSpanElement;
    private outputPowerSpan: HTMLSpanElement;
    private canvas: HTMLCanvasElement;
    private calibrationInstructionsDiv: any;
    private ctx: any;

    constructor(private document: Document, private driver: LaserDriver) {
        this.laserSwitch = <HTMLLinkElement>document.getElementById('laser-switch');
        this.modeSwitch = <HTMLLinkElement>document.getElementById('mode-switch');
        this.calibrationSpan = <HTMLLinkElement>document.getElementById('calibration');
        this.currentControlTr = <HTMLTableRowElement>document.getElementById('current-control');
        this.inputCurrentSlider = <HTMLInputElement>document.getElementById('input-current-slider');
        this.inputCurrentSpan = <HTMLSpanElement>document.getElementById('input-current');
        this.powerControlTr = <HTMLTableRowElement>document.getElementById('power-control');
        this.inputPowerSlider = <HTMLInputElement>document.getElementById('input-power-slider');
        this.inputPowerSpan = <HTMLSpanElement>document.getElementById('input-power');
        this.outputPowerSpan = <HTMLSpanElement>document.getElementById('output-power');
        this.canvas = <HTMLCanvasElement>document.getElementById('canvas');
        this.calibrationInstructionsDiv = document.getElementById('calibration-instructions');

        this.ctx = this.canvas.getContext("2d");
        this.ctx.fillStyle = 'rgb(100, 100, 100)';
        this.update();
    }

    update(): void {
        this.driver.getStatus ( (status) => {
            this.outputPowerSpan.innerHTML = Math.max(0, status.measured_power).toFixed(1).toString();
            this.ctx.clearRect(0,0,400, 15);
            this.ctx.fillRect(0, 0, status.measured_power / 10, 15);

            if (status.is_calibrated) {
                this.calibrationSpan.innerHTML = 'System calibrated';
            } else {
                this.calibrationSpan.innerHTML = 'System not calibrated';
            }

            this.inputCurrentSlider.value = status.current.toFixed(2).toString();
            this.inputCurrentSpan.innerHTML = status.current.toFixed(2).toString();

            this.inputPowerSlider.value = status.power.toFixed(2).toString();
            this.inputPowerSpan.innerHTML =  status.power.toFixed(2).toString();

            if (status.laser_on) {
                this.laserSwitch.innerHTML = 'Stop Laser';
                this.laserSwitch.className = 'btn btn-danger';
            } else {
                this.laserSwitch.innerHTML = 'Start Laser';
                this.laserSwitch.className = 'btn btn-success';
            }

            if (status.constant_power_on) {
                this.modeSwitch.innerHTML = 'Constant power';
                this.currentControlTr.style.display = 'none';
                this.powerControlTr.style.display = 'table-row';
            } else {
                this.modeSwitch.innerHTML = 'Constant current';
                this.currentControlTr.style.display = 'table-row';
                this.powerControlTr.style.display = 'none';
            }

            requestAnimationFrame( () => { this.update(); });
        });
    }

    switchLaser(): void {
        if (this.laserSwitch.innerHTML == 'Start Laser') {
            this.driver.start();
        } else { // Turn off
            this.driver.stop();
        }
    }

    switchMode(): void {
        this.driver.switchMode();
    }

    setCurrent(value: string): void {
        this.driver.setCurrent(parseFloat(value));
    }

    setPower(value: string): void {
        this.driver.setPower(parseFloat(value));
    }

    startCalibration(): void {
        this.driver.calibrate0mW();
        this.calibrationInstructionsDiv.style.display = 'table-row';
    }

    calibrationDone(): void {
        this.driver.calibrate1mW();
        this.calibrationInstructionsDiv.style.display = 'none';
    }

}