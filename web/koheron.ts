// Websocket based client for koheron-server...
// (c) Koheron

declare type Commands = HashTable<ICommand>;

interface HashTable<T> {
    [key: string]: T;
}

interface ICommand {
    name: string;
    id: number;
    args: any[]; // TODO specify array structure
    retType: string;
}

interface CmdMessage {
    devid: number;
    cmd: ICommand;
    data: Uint8Array;
}

interface Payload {
    dv: DataView;
    classId: number;
    funcId: number;
}

class Driver {

    constructor(public name: string, public id: number, private cmds: any) {}

    getCmdRef(cmdName) {
        for (let cmd of this.cmds) {
            if (cmdName === cmd.name) {
                return cmd.id;
            }
        }

        throw new ReferenceError(cmdName + ': command not found');
    }

    getCmds() {
        let cmdsDict = {};
        for (let cmd of this.cmds) {
            cmdsDict[cmd.name] = cmd;
        }
        return cmdsDict;
    }
}

class WebSocketPool {

    private pool: Array<WebSocket>;
    private freeSockets: number[];
    private dedicatedSockets: Array<WebSocket>;
    private socketCounter: number;
    private exiting: boolean;

    constructor(private poolSize: number, private url: string, private onOpenCallback: any) {
        this.poolSize = poolSize;
        this.url = url;

        this.pool = [];
        this.freeSockets = [];
        this.dedicatedSockets = [];
        this.socketCounter = 0;
        this.exiting = false;

        for (let i = 0, end = this.poolSize-1, asc = 0 <= end; asc ? i <= end : i >= end; asc ? i++ : i--) {
            var websocket = this._newWebSocket();
            this.pool.push(websocket);

            websocket.onopen = evt => {
                return this.waitForConnection(websocket, 100, () => {
                    console.assert(websocket.readyState === 1, 'Websocket not ready');
                    this.freeSockets.push(this.socketCounter);
                    if (this.socketCounter === 0) { onOpenCallback(); }
                    websocket.ID = this.socketCounter;
                    websocket.onclose = evt => {
                        setTimeout(function(){ location.reload(); }, 1000);
                    };
                    websocket.onerror = evt => {
                        console.error(`error: ${evt.data}\n`);
                        return websocket.close();
                    };

                    return this.socketCounter++;
                }
                );
            };
        }
    }

    _newWebSocket() {
        let websocket;
        if (typeof window !== 'undefined' && window !== null) {
            websocket = new WebSocket(this.url);
        } else { // Node
            let clientConfig: any = {};
            clientConfig.fragmentOutgoingMessages = false;
            let __WebSocket = require('websocket').w3cwebsocket;
            websocket = new __WebSocket(this.url, null, null, null, null, clientConfig);
        }

        websocket.binaryType = 'arraybuffer';
        return websocket;
    }

    waitForConnection(websocket, interval, callback) {
        if (websocket.readyState === 1) {
            // If the client has been set to exit while establishing the connection
            // we don't initialize the socket
            if (this.exiting) { return; }
            return callback();
        } else {
            return setTimeout( () => {
                return this.waitForConnection(websocket, interval, callback);
            }
            , interval);
        }
    }

    getSocket(sockid) {
        if (this.exiting) { return null; }
        if (!(this.freeSockets.some(x => x == sockid)) && (sockid >= 0) && (sockid < this.poolSize)) {
            return this.pool[sockid];
        } else {
            throw new ReferenceError(`Socket ID ${sockid.toString()} not available`);
        }
    }

    // Obtain a socket for a dedicated task.
    // This function is provided for long term use.
    // A new socket is opened and returned to the caller.
    // This socket is not shared and can be used for a
    // dedicated task.
    // TODO: Caller close dedicated socket
    getDedicatedSocket(callback) {
        let websocket = this._newWebSocket();
        this.dedicatedSockets.push(websocket);

        return websocket.onopen = evt => {
            return this.waitForConnection(websocket, 100, () => {
                console.assert(websocket.readyState === 1, 'Websocket not ready');

                websocket.onclose = evt => {
                    let idx = this.dedicatedSockets.indexOf(websocket);
                    if (idx > -1) { return this.dedicatedSockets.splice(idx, 1); }
                };

                websocket.onerror = evt => {
                    console.error(`error: ${evt.data}\n`);
                    return websocket.close();
                };

                return callback(websocket);
            }
            );
        };
    }

    // Retrieve a socket from the pool.
    // This function is provided for short term socket use
    // and the socket must be given back to the pool by
    // calling freeeSocket.
    requestSocket(callback) {
        if (this.exiting) {
            callback(-1);
            return;
        }

        if ((this.freeSockets != null) && (this.freeSockets.length > 0)) {
            let sockid = this.freeSockets[this.freeSockets.length-1];
            this.freeSockets.pop();
            return this.waitForConnection(this.pool[sockid], 100, () => {
                console.assert(this.pool[sockid].readyState === 1, 'Websocket not ready');
                return callback(sockid);
            }
            );
        } else { // All sockets are busy. We wait a bit and retry.
            console.assert(this.freeSockets.length === 0, 'Non empty freeSockets');
            return setTimeout( () => {
                return this.requestSocket(callback);
            }
            , 100);
        }
    }

    // Give back a socket to the pool
    freeSocket(sockid) {
        if (this.exiting) {
            return;
        }
        if (!(this.freeSockets.some(x => x == sockid)) && (sockid >= 0) && (sockid < this.poolSize)) {
            return this.freeSockets.push(sockid);
        } else {
            if (sockid != null) {
                return console.error(`Invalid Socket ID ${sockid.toString()}`);
            } else {
                return console.error('Undefined Socket ID');
            }
        }
    }

    exit() {
        this.exiting = true;
        this.freeSockets = [];
        this.socketCounter = 0;

        for (let websocket of this.pool) {
            websocket.close();
        }

        return this.dedicatedSockets.map((socket) =>
            socket.close());
    }
}

// === Helper functions to build binary buffer ===

let appendInt8 = function(buffer, value) {
    buffer.push(value & 0xff);
    return 1;
};

let appendUint8 = function(buffer, value) {
    buffer.push(value & 0xff);
    return 1;
};

let appendInt16 = function(buffer, value) {
    buffer.push((value >> 8) & 0xff);
    buffer.push(value & 0xff);
    return 2;
};

let appendUint16 = function(buffer, value) {
    buffer.push((value >> 8) & 0xff);
    buffer.push(value & 0xff);
    return 2;
};

let appendInt32 = function(buffer, value) {
    buffer.push((value >> 24) & 0xff);
    buffer.push((value >> 16) & 0xff);
    buffer.push((value >> 8) & 0xff);
    buffer.push(value & 0xff);
    return 4;
};

let appendUint32 = function(buffer, value) {
    buffer.push((value >> 24) & 0xff);
    buffer.push((value >> 16) & 0xff);
    buffer.push((value >> 8) & 0xff);
    buffer.push(value & 0xff);
    return 4;
};

let floatToBytes = function(f) {
    let buf = new ArrayBuffer(4);
    (new Float32Array(buf))[0] = f;
    return (new Uint32Array(buf))[0];
};

let bytesTofloat = function(bytes) {
    let buffer = new ArrayBuffer(4);
    (new Uint32Array(buffer))[0] = bytes;
    return new Float32Array(buffer)[0];
};

let appendFloat32 = (buffer, value) => appendUint32(buffer, floatToBytes(value));

let appendFloat64 = function(buffer, value) {
    let buf = new ArrayBuffer(8);
    (new Float64Array(buf))[0] = value;
    appendUint32(buffer, (new Uint32Array(buf, 4))[0]);
    appendUint32(buffer, (new Uint32Array(buf, 0))[0]);
    console.assert(buf.byteLength === 8, 'Invalid float64 size');
    return buf.byteLength;
};

let appendArray = function(buffer, array) {
    let bytes = new Uint8Array(array.buffer);
    return bytes.map((_byte) => buffer.push(_byte));
};

let appendVector = function(buffer, array) {
    appendUint32(buffer, array.buffer.byteLength);
    let bytes = new Uint8Array(array.buffer);
    return bytes.map((_byte) => buffer.push(_byte));
};

let appendString = function(buffer, str: string) {
    appendUint32(buffer, str.length);
    for (let i = 0; i < str.length; i++) {
        buffer.push(str.charCodeAt(i));
    }
};

let isStdArray = type => type.split('<')[0].trim() === 'std::array';

let isStdVector = type => type.split('<')[0].trim() === 'std::vector';

let isStdString = type => type.trim() === 'std::string';

let isStdTuple = type => type.split('<')[0].trim() === 'std::tuple';

let getStdVectorType = type => type.split('<')[1].split('>')[0].trim();

function Command(devId: number, cmd: ICommand, ...params: any[]): CmdMessage {
    let buffer = [];
    appendUint32(buffer, 0); // RESERVED
    appendUint16(buffer, devId);
    appendUint16(buffer, cmd.id);

    if (cmd.args.length === 0) {
        return {
            devid: devId,
            cmd,
            data: new Uint8Array(buffer)
        };
    }

    if (cmd.args.length !== params.length) {
        throw new Error(`Invalid number of arguments. Expected ${cmd.args.length} receive ${params.length}.`);
    }

    let buildPayload = function(args, params) {
        if (args == null) { args = []; }
        let payload = [];

        for (let i = 0; i < args.length; i++) {
            let arg = args[i];
            switch (arg.type) {
                case 'uint8_t':
                    appendUint8(payload, params[i]);
                    break;
                case 'int8_t':
                    appendInt8(payload, params[i]);
                    break;
                case 'uint16_t':
                    appendUint16(payload, params[i]);
                    break;
                case 'int16_t':
                    appendInt16(payload, params[i]);
                    break;
                case 'uint32_t':
                    appendUint32(payload, params[i]);
                    break;
                case 'int32_t':
                    appendInt32(payload, params[i]);
                    break;
                case 'float':
                    appendFloat32(payload, params[i]);
                    break;
                case 'double':
                    appendFloat64(payload, params[i]);
                    break;
                case 'bool':
                    if (params[i]) {
                        appendUint8(payload, 1);
                    } else {
                        appendUint8(payload, 0);
                    }
                    break;
                default:
                    if (isStdArray(arg.type)) {
                        appendArray(payload, params[i]);
                    } else if (isStdVector(arg.type)) {
                        appendVector(payload, params[i]);
                    } else if (isStdString(arg.type)) {
                        appendString(payload, params[i]);
                    } else {
                        throw new TypeError(`Unknown type ${arg.type}`);
                    }
            }
        }

        return payload;
    };

    return {
        devid: devId,
        cmd,
        data: new Uint8Array(buffer.concat(buildPayload(cmd.args, params)))
    };
}

class Client {

    private url: string;
    private driversList: Array<Driver>;
    private websockpool: WebSocketPool;

    constructor(private IP: string, private websockPoolSize: number) {
        if (websockPoolSize == null) { websockPoolSize = 5; }
        this.websockPoolSize = websockPoolSize;
        this.url = `ws://${IP}:8080`;
        this.driversList = [];
    }

    init(callback) {
        return this.websockpool = new WebSocketPool(this.websockPoolSize, this.url, (function() {
            return this.loadCmds(callback);
        }.bind(this)));
    }

    exit() {
        this.websockpool.exit();
        return delete this.websockpool;
    }

    // ------------------------
    //  Send
    // ------------------------

    send(cmd: CmdMessage) {
        if (this.websockpool === null || typeof this.websockpool === 'undefined') { return; }

        this.websockpool.requestSocket( (sockid: number) => {
            if (sockid < 0) { return; }
            let websocket = this.websockpool.getSocket(sockid);
            websocket.send(cmd.data);
            if (this.websockpool !== null && typeof this.websockpool !== 'undefined') {
                 this.websockpool.freeSocket(sockid);
            }
        });
    }

    // ------------------------
    //  Receive
    // ------------------------

    redirectError(errTest, errMsg, onSuccess, onError) {
        if (errTest) {
            if (onError === null) {
                throw new TypeError(errMsg);
            } else {
                return onError(errMsg);
            }
        } else {
            return onSuccess();
        }
    }

    getPayload(
        mode: 'static' | 'dynamic',
        evt: MessageEvent<ArrayBuffer>
    ): Payload {
        const STATIC_HEADER_BYTES  = 8;   // reserved(4) + classId(2) + funcId(2)
        const DYNAMIC_HEADER_BYTES = 12;  // + length(4) at offset 8

        const buf = evt.data;
        if (!(buf instanceof ArrayBuffer)) {
            // If this ever triggers, ensure: websocket.binaryType = 'arraybuffer'
            throw new Error('Expected ArrayBuffer in MessageEvent.data');
        }

        const dv = new DataView(buf);
        if (dv.byteLength < STATIC_HEADER_BYTES) {
            throw new Error(`Frame too small: ${dv.byteLength} < ${STATIC_HEADER_BYTES}`);
        }

        // Header
        // reserved is available if you need it later:
        // const reserved = dv.getUint32(0);
        const classId = dv.getUint16(4);
        const funcId  = dv.getUint16(6);

        let offset: number;
        let len: number;

        if (mode === 'static') {
            offset = STATIC_HEADER_BYTES;
            len = dv.byteLength - STATIC_HEADER_BYTES;

            if (len < 0) {
                throw new Error('Negative payload length (static).');
            }
        } else {
            if (dv.byteLength < DYNAMIC_HEADER_BYTES) {
                throw new Error(`Frame too small for dynamic header: ${dv.byteLength} < ${DYNAMIC_HEADER_BYTES}`);
            }

            len = dv.getUint32(8);

            if (dv.byteLength !== len + DYNAMIC_HEADER_BYTES) {
                throw new Error(`Bad dynamic length: expected ${len + DYNAMIC_HEADER_BYTES}, got ${dv.byteLength}`);
            }

            offset = DYNAMIC_HEADER_BYTES;
        }

        const payloadView = new DataView(buf, offset, len);
        return { dv: payloadView, classId, funcId };
    }

    private _readBaseAsync(
        mode: 'static' | 'dynamic',
        cmd: CmdMessage
    ): Promise<DataView> {
        return new Promise<DataView>((resolve, reject) => {
            if (!this.websockpool) {
                return reject(new Error('No websocket pool'));
            }

            let sockid = -1;

            const cleanup = (
                websocket?: WebSocket,
                onMessage?: (ev: MessageEvent) => void,
                onError?: (ev: Event) => void,
                onClose?: () => void
            ) => {
                if (websocket && onMessage) {
                    websocket.removeEventListener('message', onMessage);
                }

                if (websocket && onError) {
                    websocket.removeEventListener('error', onError);
                }

                if (websocket && onClose) {
                    websocket.removeEventListener('close', onClose);
                }

                if (sockid >= 0 && this.websockpool) {
                    this.websockpool.freeSocket(sockid);
                }
            };

            this.websockpool.requestSocket((id: number) => {
                if (id < 0) {
                    return reject(new Error('Failed to acquire socket'));
                }

                sockid = id;

                const websocket: WebSocket = this.websockpool.getSocket(sockid);
                try { (websocket as any).binaryType = 'arraybuffer'; } catch {}

                const onMessage = (evt: MessageEvent) => {
                    try {
                        const payload = this.getPayload(mode, evt);
                        cleanup(websocket, onMessage, onError, onClose);
                        resolve(payload.dv);
                    } catch (e) {
                        cleanup(websocket, onMessage, onError, onClose);
                        reject(e instanceof Error ? e : new Error(String(e)));
                    }
                };

                const onError = (_e: Event) => {
                    cleanup(websocket, onMessage, onError, onClose);
                    reject(new Error('WebSocket error'));
                };

                const onClose = () => {
                    cleanup(websocket, onMessage, onError, onClose);
                    reject(new Error('WebSocket closed before response'));
                };

                websocket.addEventListener('message', onMessage, { once: true });
                websocket.addEventListener('error', onError, { once: true });
                websocket.addEventListener('close', onClose, { once: true });
                websocket.send(cmd.data);
            });
        });
    }
    
    // ---- tiny helper to support both callback and Promise APIs ----
    private _dual<T>(
        producer: () => Promise<T>,
        cb?: (x: T) => void
    ): Promise<T> | void {
        if (cb) {
            producer().then(cb).catch(() => cb(null as any));
            return;
        }
        return producer();
    }

    readUint32Array(cmd: CmdMessage, fn: (x: Uint32Array) => void): void;
    readUint32Array(cmd: CmdMessage): Promise<Uint32Array>;
    readUint32Array(cmd: CmdMessage, fn?: (x: Uint32Array) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return new Uint32Array(dv.buffer, dv.byteOffset, dv.byteLength / 4);
        }, fn);
    }

    readInt32Array(cmd: CmdMessage, fn: (x: Int32Array) => void): void;
    readInt32Array(cmd: CmdMessage): Promise<Int32Array>;
    readInt32Array(cmd: CmdMessage, fn?: (x: Int32Array) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return new Int32Array(dv.buffer, dv.byteOffset, dv.byteLength / 4);
        }, fn);
    }

    readFloat32Array(cmd: CmdMessage, fn: (x: Float32Array) => void): void;
    readFloat32Array(cmd: CmdMessage): Promise<Float32Array>;
    readFloat32Array(cmd: CmdMessage, fn?: (x: Float32Array) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return new Float32Array(dv.buffer, dv.byteOffset, dv.byteLength / 4);
        }, fn);
    }

    readFloat64Array(cmd: CmdMessage, fn: (x: Float64Array) => void): void;
    readFloat64Array(cmd: CmdMessage): Promise<Float64Array>;
    readFloat64Array(cmd: CmdMessage, fn?: (x: Float64Array) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return new Float64Array(dv.buffer.slice(dv.byteOffset, dv.byteOffset + dv.byteLength));
        }, fn);
    }

    readUint32Vector(cmd: CmdMessage, fn: (x: Uint32Array) => void): void;
    readUint32Vector(cmd: CmdMessage): Promise<Uint32Array>;
    readUint32Vector(cmd: CmdMessage, fn?: (x: Uint32Array) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('dynamic', cmd);
            return new Uint32Array(dv.buffer, dv.byteOffset, dv.byteLength / 4);
        }, fn);
    }

    readFloat32Vector(cmd: CmdMessage, fn: (x: Float32Array) => void): void;
    readFloat32Vector(cmd: CmdMessage): Promise<Float32Array>;
    readFloat32Vector(cmd: CmdMessage, fn?: (x: Float32Array) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('dynamic', cmd);
            return new Float32Array(dv.buffer, dv.byteOffset, dv.byteLength / 4);
        }, fn);
    }

    readFloat64Vector(cmd: CmdMessage, fn: (x: Float64Array) => void): void;
    readFloat64Vector(cmd: CmdMessage): Promise<Float64Array>;
    readFloat64Vector(cmd: CmdMessage, fn?: (x: Float64Array) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('dynamic', cmd);
            return new Float64Array(dv.buffer.slice(dv.byteOffset, dv.byteOffset + dv.byteLength));
        }, fn);
    }

    readUint32(cmd: CmdMessage, fn: (x: number) => void): void;
    readUint32(cmd: CmdMessage): Promise<number>;
    readUint32(cmd: CmdMessage, fn?: (x: number) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return dv.getUint32(0);
        }, fn);
    }

    readInt32(cmd: CmdMessage, fn: (x: number) => void): void;
    readInt32(cmd: CmdMessage): Promise<number>;
    readInt32(cmd: CmdMessage, fn?: (x: number) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return dv.getInt32(0);
        }, fn);
    }

    readFloat32(cmd: CmdMessage, fn: (x: number) => void): void;
    readFloat32(cmd: CmdMessage): Promise<number>;
    readFloat32(cmd: CmdMessage, fn?: (x: number) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return dv.getFloat32(0);
        }, fn);
    }

    readFloat64(cmd: CmdMessage, fn: (x: number) => void): void;
    readFloat64(cmd: CmdMessage): Promise<number>;
    readFloat64(cmd: CmdMessage, fn?: (x: number) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return dv.getFloat64(0);
        }, fn);
    }

    readBool(cmd: CmdMessage, fn: (x: boolean) => void): void;
    readBool(cmd: CmdMessage): Promise<boolean>;
    readBool(cmd: CmdMessage, fn?: (x: boolean) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return dv.getUint8(0) === 1;
        }, fn);
    }

    readTuple(cmd: CmdMessage, fmt: string, fn: (x: any[]) => void): void;
    readTuple(cmd: CmdMessage, fmt: string): Promise<any[]>;
    readTuple<T extends any[]>(cmd: CmdMessage, fmt: string): Promise<T>;
    readTuple(cmd: CmdMessage, fmt: string, fn?: (x: any[]) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('static', cmd);
            return this.deserialize(fmt, dv);
        }, fn);
    }

    deserialize(fmt: string, dv: DataView, onError?: any) {
        if (onError == null) { onError = null; }
        let tuple = [];
        let offset = 0;

        for (let i = 0, end = fmt.length-1, asc = 0 <= end; asc ? i <= end : i >= end; asc ? i++ : i--) {
            switch (fmt[i]) {
                case 'B':
                    tuple.push(dv.getUint8(offset));
                    offset += 1;
                    break;
                case 'b':
                    tuple.push(dv.getInt8(offset));
                    offset += 1;
                    break;
                case 'H':
                    tuple.push(dv.getUint16(offset));
                    offset += 2;
                    break;
                case 'h':
                    tuple.push(dv.getInt16(offset));
                    offset += 2;
                    break;
                case 'I':
                    tuple.push(dv.getUint32(offset));
                    offset += 4;
                    break;
                case 'i':
                    tuple.push(dv.getInt32(offset));
                    offset += 4;
                    break;
                case 'f':
                    tuple.push(dv.getFloat32(offset));
                    offset += 4;
                    break;
                case 'd':
                    tuple.push(dv.getFloat64(offset));
                    offset += 8;
                    break;
                case '?':
                    if (dv.getUint8(offset) === 0) {
                        tuple.push(false);
                    } else {
                        tuple.push(true);
                    }
                    offset += 1;
                    break;
                default:
                    this.redirectError(true, `Unknown or unsupported type ${fmt[i]}`, (function() {}), onError);
            }
        }


        return tuple;
    }

    parseString(data: DataView, offset?: number) {
        if (offset == null) { offset = 0; }
        return (__range__(0, data.byteLength - offset - 1, true).map((i) => (String.fromCharCode(data.getUint8(offset + i))))).join('');
    }

    readString(cmd: CmdMessage, fn: (str: string) => void): void;
    readString(cmd: CmdMessage): Promise<string>;
    readString(cmd: CmdMessage, fn?: (str: string) => void) {
        return this._dual(async () => {
            const dv = await this._readBaseAsync('dynamic', cmd);
            return this.parseString(dv);
        }, fn);
    }

    readJSON(cmd: CmdMessage, fn: (json: any) => void): void;
    readJSON(cmd: CmdMessage): Promise<any>;
    readJSON(cmd: CmdMessage, fn?: (json: any) => void) {
        return this._dual(async () => {
            const str = await this.readString(cmd) as string;
            return JSON.parse(str);
        }, fn);
    }

    // ------------------------
    //  Drivers
    // ------------------------

    loadCmds(callback: () => void) {
        this.readJSON(Command(1, <ICommand>{'id': 1, 'args': []}), data => {
            for (let driver of data) {
                let dev = new Driver(driver.class, driver.id, driver.functions);
                // dev.show()
                this.driversList.push(dev);
            }
            callback();
        });
    }

    getDriver(name: string) {
        for (let driver of this.driversList) {
            if (driver.name === name) {
                return driver;
            }
        }
        throw new Error(`Driver ${name} not found`);
    }

    getDriverById(id) {
        for (let driver of this.driversList) {
            if (driver.id === id) { return driver; }
        }

        throw new ReferenceError(`Driver ID ${id} not found`);
    }
};

function __range__(left, right, inclusive) {
  let range = [];
  let ascending = left < right;
  let end = !inclusive ? right : ascending ? right + 1 : right - 1;
  for (let i = left; ascending ? i < end : i > end; ascending ? i++ : i--) {
      range.push(i);
  }
  return range;
}

class Imports {
    private importLinks: HTMLLinkElement[];

    constructor (document: Document) {
        this.importLinks = <HTMLLinkElement[]><any>document.querySelectorAll('link[rel="import"]');
        for (let i = 0; i < this.importLinks.length ; i++) {
            let template = (this.importLinks[i] as any).import.querySelector('.template');
            let clone = document.importNode(template.content, true);
            let parentId = this.importLinks[i].dataset.parent;
            document.getElementById(parentId).appendChild(clone);
        }
    }
}
