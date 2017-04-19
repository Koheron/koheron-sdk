declare interface HashTable<T> {
    [key: string]: T;
}

declare interface ICommand {
    name: string;
    id: number;
    args: any[]; // TODO specify array structure
    retType: string;
}

declare type Commands = HashTable<ICommand>;

declare class Driver {
    public driver_name: string;
    public id: number;
    private cmds: ICommand;

    getCmds(): Commands;
}

interface CmdMessage {
    devid: number;
    cmd: ICommand;
    data: Uint8Array;
}

declare class WebSocketPool {
    getDedicatedSocket(callback: (socket: WebSocket) => void): void;
    waitForConnection(websocket: WebSocket, interval: number, callback: () => void): void;
    requestSocket(callback: (sockid: number) => void): void;
    getSocket(sockid: number): WebSocket;
    freeSocket(sockid: number): void;
}

declare function Command(id: number, cmd: ICommand, ...params: any[]): CmdMessage;

declare class Client {
    public websockpool: WebSocketPool;

    constructor(IP: string, websockPoolSize?: number);

    init(callback: () => void): void;
    exit(): void;

    getDriver(driver_name: string): Driver;

    getPayload(mode: string, evt: MessageEvent): any[];

    send(cmd: CmdMessage): void;
    readUint32Array(cmd: CmdMessage, fn: (array: Uint32Array) => void): void;
    readFloat32Array(cmd: CmdMessage, fn: (array: Float32Array) => void): void;
    readUint32Vector(cmd: CmdMessage, fn: (array: Uint32Array) => void): void;
    readFloat32Vector(cmd: CmdMessage, fn: (array: Float32Array) => void): void;
    readUint32(cmd: CmdMessage, fn: (u32: number) => void): void;
    readInt32(cmd: CmdMessage, fn: (i32: number) => void): void;
    readFloat32(cmd: CmdMessage, fn: (f32: number) => void): void;
    readFloat64(cmd: CmdMessage, fn: (f64: number) => void): void;
    readBool(cmd: CmdMessage, fn: (bool: boolean) => void): void;
    readTuple(cmd: CmdMessage, fmt: string, fn: (tup: any[]) => void): void;
    readString(cmd: CmdMessage, fn: (str: string) => void): void;
    readJSON(cmd: CmdMessage, fn: (json: any) => void): void;
}