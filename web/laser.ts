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
    private powerControl: any;
    private measuredPowerSpan: HTMLSpanElement;
    private canvas: HTMLCanvasElement;
    private calibrationInstructionsDiv: any;
    private ctx: any;

    private laserControlInputs: HTMLInputElement[];

    constructor(private document: Document, private driver: LaserDriver) {
        this.laserSwitch = <HTMLInputElement>document.getElementById('laser-switch');
        this.laserModeSelect = <HTMLSelectElement>document.getElementById('laser-mode');
        this.calibrationSpan = <HTMLSpanElement>document.getElementById('calibration');
        this.currentControl = document.getElementsByClassName('current-control');
        this.powerControl = document.getElementsByClassName('power-control');
        this.measuredPowerSpan = <HTMLSpanElement>document.getElementById('measured-power');
        this.canvas = <HTMLCanvasElement>document.getElementById('canvas');
        this.calibrationInstructionsDiv = document.getElementById('calibration-instructions');

        this.laserControlInputs = <HTMLInputElement[]><any>document.getElementsByClassName("laser-control-input");

        let canvasWidth: number = (<HTMLInputElement>document.querySelector(".laser-control-input[type='range']")).offsetWidth;
        this.canvas.width = canvasWidth;

        this.ctx = this.canvas.getContext("2d");
        this.ctx.fillStyle = 'rgb(100, 100, 100)';

        this.update();
        this.initLaserSwitch();
        this.initLaserControlInputs();
        this.initStartCalibration();
        this.initEndCalibration();

    }

    update(): void {
        this.driver.getStatus ( (status) => {

            this.measuredPowerSpan.innerHTML = Math.max(0, status.measured_power).toFixed(1);
            this.ctx.clearRect(0, 0, this.canvas.width, 15);
            this.ctx.fillRect(0, 0, status.measured_power * this.canvas.width / 4000, 15);

            if (status.is_calibrated) {
                this.calibrationSpan.innerHTML = 'Status: Calibrated';
            } else {
                this.calibrationSpan.innerHTML = 'Status: Not calibrated';
            }

            let laserControlInputsArray = [];
            for (let j = 0; j < this.laserControlInputs.length; j++) {
                laserControlInputsArray.push(this.laserControlInputs[j]);
            }

            if (laserControlInputsArray.indexOf(<HTMLInputElement>document.activeElement) == -1) {
                for (let j = 0; j < this.laserControlInputs.length; j++) {
                    if (this.laserControlInputs[j].dataset.command === "setCurrent") {
                        this.laserControlInputs[j].value = status.current.toFixed(2);
                    } else if (this.laserControlInputs[j].dataset.command === "setPower") {
                        this.laserControlInputs[j].value = status.power.toFixed(1);
                    }
                }
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

    initLaserSwitch(): void {
        this.laserSwitch.addEventListener('change', (event) => {
            if (this.laserOn) {
                this.driver.stop();
                this.laserOn = false;
            } else {
                this.driver.start();
                this.laserOn = true;
            }
        })
    }

    switchMode(): void {
        this.driver.switchMode();
    }

    initLaserControlInputs(): void {
        let events = ['change', 'input'];
        for (let j = 0; j < events.length; j++) {
            for (let i = 0; i < this.laserControlInputs.length; i++) {
                this.laserControlInputs[i].addEventListener(events[j], (event) => {
                    let counterType: string = "number";
                    if ((<HTMLInputElement>event.currentTarget).type == "number") {
                        counterType = "range";
                    }
                    let command = (<HTMLInputElement>event.currentTarget).dataset.command;
                    let value = (<HTMLInputElement>event.currentTarget).value;
                    (<HTMLInputElement>document.querySelector(".laser-control-input[data-command='" + command + "'][type='" + counterType + "']")).value = value ;
                    this.driver[command](parseFloat(value));
                })
            }
        }

    }

    initStartCalibration(): void {
        let startCalibrationBtn = <HTMLButtonElement>document.getElementById("start-calibration-btn");
        startCalibrationBtn.addEventListener('click', (event) => {
            this.driver.calibrate0mW();
            this.calibrationSpan.style.display = "none";
            this.calibrationInstructionsDiv.style.display = 'block';
        })
    }

    initEndCalibration(): void {
        let endCalibrationBtn = <HTMLButtonElement>document.getElementById("end-calibration-btn");
        endCalibrationBtn.addEventListener('click', (event) => {
            this.driver.calibrate1mW();
            this.calibrationSpan.style.display = "inline";
            this.calibrationInstructionsDiv.style.display = 'none';
        })
    }



}