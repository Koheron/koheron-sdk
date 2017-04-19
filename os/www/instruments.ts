// (c) Koheron
// Instruments

class Instruments {

    private ip: string;
    public instruments: string[];
    public liveInstrument: string;
    public isNewInstrument: boolean;

    constructor () {
        this.checkForNewInstrument();
    }

    checkForNewInstrument() {
        this.getInstrumentsStatus( (status) => {
            this.isNewInstrument = false;
            if (JSON.stringify(this.instruments) !== JSON.stringify(status['instruments'])) {
                this.instruments = status['instruments'];
                this.isNewInstrument = true;
            }
            if (JSON.stringify(this.liveInstrument) !== JSON.stringify(status['live_instrument'])) {
                this.liveInstrument = status['live_instrument'];
                this.isNewInstrument = true;
            }
        });
        setTimeout( () => {
            this.checkForNewInstrument(), 500
        });
    }

    getInstrumentsStatus(cb: (status: any) => void) : void {
        let xhr = new XMLHttpRequest();
        xhr.open('GET', '/api/instruments', true);
        xhr.onload = () => {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    cb(JSON.parse(xhr.responseText))
                }
                else {
                    throw "Cannot retrieve local instruments";
                }
            }
        }
        xhr.send(null);
    }

    runInstrument(name: string, cb: (status: boolean) => void) : void {
        let xhr = new XMLHttpRequest();
        xhr.open('GET', '/api/instruments/run/' + name, true);
        xhr.onload = () => {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    cb(false);
                }
                else {
                    cb(true);
                }
            }
        }
        xhr.send(null);
    }

    deleteInstrument(name: string) : void {
        let xhr = new XMLHttpRequest();
        xhr.open('GET', '/api/instruments/delete/' + name, true);
        xhr.send(null);
    }

}