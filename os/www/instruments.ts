// (c) Koheron
// Instruments

class Instruments {

    private ip: string;
    public instruments: string[];
    public liveInstrument: string;
    public isUpdate: boolean;

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

    runInstrument(name: string, callback: (status: boolean) => void) : void {
        let xhr = new XMLHttpRequest();
        xhr.open('GET', '/api/instruments/run/' + name, true);
        xhr.onload = () => {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    callback(false);
                }
                else {
                    callback(true);
                }
            }
        }
        xhr.send(null);
    }

    uploadInstrument(file: File, callback: (status: boolean) => void): void {

        let formData = new FormData();
        formData.append(file.name, file);

        let xhr = new XMLHttpRequest();
        xhr.open('POST', '/api/instruments/upload', true);

        xhr.send(formData);

        xhr.onload = () => {

            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    callback(true);
                }
                else {
                    callback(false);
                };
            }
        }
    }

    deleteInstrument(name: string) : void {
        let xhr = new XMLHttpRequest();
        xhr.open('GET', '/api/instruments/delete/' + name, true);
        xhr.send(null);
    }

}