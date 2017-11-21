// (c) Koheron
// Instruments

class Instruments {

    private ip: string;
    public instruments: string[];
    public liveInstrument: string;
    public isNewInstrument: boolean;

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