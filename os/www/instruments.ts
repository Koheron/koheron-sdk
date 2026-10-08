// HTTP transport for the OS instrument manager.
class Instruments {
    private request(method: string, path: string, body: FormData | null,
                    callback: (error: string | null, text?: string) => void): void {
        const xhr = new XMLHttpRequest();
        xhr.open(method, '/api/instruments/' + path, true);
        xhr.timeout = 120000;
        xhr.onload = () => callback(xhr.status === 200 ? null :
            `Request failed (HTTP ${xhr.status}).`, xhr.responseText);
        xhr.onerror = () => callback('Cannot reach the board. Check the connection and refresh.');
        xhr.ontimeout = () => callback('Request timed out. Refresh to check the board before trying again.');
        xhr.send(body);
    }

    getInstrumentsStatus(callback: (status: any) => void,
                         onError: (error: string) => void = () => {}): void {
        this.request('GET', 'details', null, (error, text) => {
            if (error) { onError(error); return; }
            let status: any;
            try {
                status = JSON.parse(text);
                if (!status || !Array.isArray(status.instruments)) { throw new Error(); }
            } catch (_) { onError('Invalid instrument status received.'); return; }
            callback(status);
        });
    }

    runInstrument(name: string, callback: (failed: boolean, error?: string) => void): void {
        this.request('GET', 'run/' + encodeURIComponent(name), null,
            error => callback(error !== null, error));
    }

    uploadInstrument(file: File, callback: (success: boolean, error?: string) => void): void {
        const body = new FormData();
        body.append(file.name, file);
        this.request('POST', 'upload', body, error => callback(error === null, error));
    }

    deleteInstrument(name: string, callback: (success: boolean, error?: string) => void): void {
        this.request('GET', 'delete/' + encodeURIComponent(name), null,
            error => callback(error === null, error));
    }
}
