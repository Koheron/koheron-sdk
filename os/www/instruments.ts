// HTTP transport for the OS instrument manager.
class Instruments {
    private request(method: string, path: string, body: FormData | null,
                    callback: (error: string | null, text?: string) => void): void {
        const xhr = new XMLHttpRequest();
        xhr.open(method, '/api/instruments/' + path, true);
        xhr.timeout = 300000;
        xhr.onload = () => {
            let error = xhr.status === 200 ? null : `Request failed (HTTP ${xhr.status}).`;
            if (error) {
                try { const details = JSON.parse(xhr.responseText); if (typeof details.error === 'string') { error = details.error; } } catch (_) {}
            }
            callback(error, xhr.responseText);
        };
        xhr.onerror = () => callback('Cannot reach the board. Check the connection and refresh.');
        xhr.ontimeout = () => callback('Request timed out. Refresh to check the board before trying again.');
        xhr.send(body);
    }

    private json(method: string, path: string, system = false): Promise<any> {
        return new Promise((resolve, reject) => {
            const xhr = new XMLHttpRequest();
            xhr.open(method, '/api/' + (system ? 'system/' : 'instruments/') + path, true);
            xhr.timeout = 300000;
            xhr.onload = () => {
                let value: any;
                try { value = JSON.parse(xhr.responseText); }
                catch (_) { reject(new Error(`Invalid board response (HTTP ${xhr.status}).`)); return; }
                if (xhr.status !== 200) { reject(new Error(value.error || `Request failed (HTTP ${xhr.status}).`)); return; }
                resolve(value);
            };
            xhr.onerror = () => reject(new Error('Cannot reach the board. Check the connection and refresh.'));
            xhr.ontimeout = () => reject(new Error('Request timed out. Check the live status before trying again.'));
            xhr.send(null);
        });
    }
    getRuntimeStatus(): Promise<RuntimeStatus> { return this.json('GET', 'status', true); }
    preflight(name: string): Promise<any> { return this.json('GET', 'preflight/' + encodeURIComponent(name)); }
    activate(name: string): Promise<any> { return this.json('POST', 'activate/' + encodeURIComponent(name)); }
    setDefault(name: string): Promise<any> { return this.json('POST', 'default/' + encodeURIComponent(name)); }
    control(action: 'start' | 'stop' | 'restart'): Promise<any> { return this.json('POST', 'control/' + action); }

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
