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
    private laserSwitch: HTMLInputElement;
    private laserOn: boolean;

    private laserModeSelect: HTMLSelectElement;
    private calibrationSpan: HTMLSpanElement;
    private currentControl: any;
    private inCurrentSlider: HTMLInputElement;
    private inCurrentInput: HTMLInputElement;
    private powerControl: any;
    private inPowerInput: HTMLInputElement;
    private inPowerSlider: HTMLInputElement;
    private measuredPowerSpan: HTMLSpanElement;
    private canvas: HTMLCanvasElement;
    private calibrationInstructionsDiv: any;
    private ctx: any;

    constructor(private document: Document, private driver: LaserDriver) {
        this.laserSwitch = <HTMLInputElement>document.getElementById('laser-switch');
        this.laserModeSelect = <HTMLSelectElement>document.getElementById('laser-mode');
        this.calibrationSpan = <HTMLLinkElement>document.getElementById('calibration');
        this.currentControl = document.getElementsByClassName('current-control');
        this.inCurrentSlider = <HTMLInputElement>document.getElementById('in-current-slider');
        this.inCurrentInput = <HTMLInputElement>document.getElementById('in-current-input');
        this.powerControl = document.getElementsByClassName('power-control');
        this.inPowerSlider = <HTMLInputElement>document.getElementById('in-power-slider');
        this.inPowerInput = <HTMLInputElement>document.getElementById('in-power-input');
        this.measuredPowerSpan = <HTMLSpanElement>document.getElementById('measured-power');
        this.canvas = <HTMLCanvasElement>document.getElementById('canvas');
        this.calibrationInstructionsDiv = document.getElementById('calibration-instructions');

        let canvasWidth: number = this.inCurrentSlider.offsetWidth;
        this.canvas.width = canvasWidth;

        this.ctx = this.canvas.getContext("2d");
        this.ctx.fillStyle = 'rgb(100, 100, 100)';

        this.update();
    }

    update(): void {
        this.driver.getStatus ( (status) => {

            this.measuredPowerSpan.innerHTML = Math.max(0, status.measured_power).toFixed(1).toString();
            this.ctx.clearRect(0, 0, this.canvas.width, 15);
            this.ctx.fillRect(0, 0, status.measured_power * this.canvas.width / 4000, 15);

            if (status.is_calibrated) {
                this.calibrationSpan.innerHTML = 'Status: Calibrated';
            } else {
                this.calibrationSpan.innerHTML = 'Status: Not calibrated';
            }

            if (document.activeElement !== this.inCurrentInput) {
                this.inCurrentInput.value = status.current.toFixed(2).toString();
            }
            if (document.activeElement !== this.inCurrentSlider) {
                this.inCurrentSlider.value = status.current.toFixed(2).toString();
            }

            if (document.activeElement !== this.inPowerInput) {
                this.inPowerInput.value = status.power.toFixed(1).toString();
            }
            if (document.activeElement !== this.inPowerSlider) {
                this.inPowerSlider.value = status.power.toFixed(1).toString();
            }

            this.laserOn = status.laser_on;
            if (this.laserOn) {
                this.laserSwitch.checked = true;
            } else {
                this.laserSwitch.checked = false;
            }

            if (status.constant_power_on) {
                this.laserModeSelect.value = "power";
                for (let i: number = 0; i < this.currentControl.length; i++) {
                    this.currentControl[i].style.display = 'none';
                }
                for (let i: number = 0; i < this.powerControl.length; i++) {
                    this.powerControl[i].style.display = 'table-cell';
                }
            } else {
                this.laserModeSelect.value = 'current';
                for (let i: number = 0; i < this.currentControl.length; i++) {
                    this.currentControl[i].style.display = 'table-cell';
                }
                for (let i: number = 0; i < this.powerControl.length; i++) {
                    this.powerControl[i].style.display = 'none';
                }

            }

            requestAnimationFrame( () => { this.update(); });
        });
    }

    switchLaser(): void {
        if (this.laserOn) {
            this.driver.stop();
            this.laserOn = false;
        } else {
            this.driver.start();
            this.laserOn = true;
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
        this.calibrationSpan.style.display = "none";
        this.calibrationInstructionsDiv.style.display = 'block';
    }

    calibrationDone(): void {
        this.driver.calibrate1mW();
        this.calibrationSpan.style.display = "inline";
        this.calibrationInstructionsDiv.style.display = 'none';
    }

}