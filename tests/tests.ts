// Websocket client tests
// (c) Koheron

// http://stackoverflow.com/questions/19971713/nodeunit-runtime-thrown-errors-in-test-function-are-silent/20038890#20038890
'use strict';

//process.on('uncaughtException', err => console.error(err.stack));

const HOST = process.env.HOST;

// TODO fix ugly hack
//import {Command} from '../web/koheron';
//import {Client} from '../web/koheron';
import {Command} from '../tmp/koheron_with_exports';
import {Client} from '../tmp/koheron_with_exports';

class Tests {

    private id: number;
    private cmds: any;
    private driver: any;

    constructor(private client: Client) {
        this.client = client;
        this.driver = this.client.getDriver("Tests");
        this.id = this.driver.id;
        this.cmds = this.driver.getCmds();
    }

    setScalars(a, b, c, d, e, f, cb) {
        this.client.readBool(Command(this.id, this.cmds.set_scalars, a, b, c, d, e, f), cb);
    }

    setArray(cb) {
        let u = 4223453;
        let f = 3.14159265359;
        let d = 2.654798454646;
        let i = -56789;
        let array = new Uint32Array(8192);
        for (let _i = 0, end = array.length - 1, asc = 0 <= end; asc ? _i <= end : _i >= end; asc ? _i++ : _i--) { array[_i] = _i; }
        this.client.readBool(Command(this.id, this.cmds.set_array, u, f, array, d, i), cb);
    }

    getArray(cb) {
        this.client.readUint32Array(Command(this.id, this.cmds.get_array), cb);
    }

    getVector(cb) {
        this.client.readFloat32Vector(Command(this.id, this.cmds.get_vector), cb);
    }

    getConstVector(cb) {
        this.client.readUint32Vector(Command(this.id, this.cmds.get_const_vector), cb);
    }

    getConstAutoVector(cb) {
        this.client.readUint32Vector(Command(this.id, this.cmds.get_const_auto_vector), cb);
    }

    setString(cb) {
        this.client.readBool(Command(this.id, this.cmds.set_string, 'Hello World'), cb);
    }

    getString(cb) {
        this.client.readString(Command(this.id, this.cmds.get_string), cb);
    }

    getConstString(cb) {
        this.client.readString(Command(this.id, this.cmds.get_const_string), cb);
    }

    getJSON(cb) {
        this.client.readJSON(Command(this.id, this.cmds.get_json), cb);
    }

    getTuple(cb) {
        this.client.readTuple(Command(this.id, this.cmds.get_tuple), 'Idd?', cb);
    }
}

// // Unit tests

export function setScalars(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.setScalars(429496729, -2048, 3.14159265358, true, 2.718281828459045, 42, (is_ok) => {
                assert.ok(is_ok);
                client.exit();
                assert.done();
            });
        });
    });
}

export function setArray(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.setArray( (is_ok) => {
                assert.ok(is_ok);
                client.exit();
                assert.done();
            });
        });
    });
}

export function getArray(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.getArray( (array) => {
                assert.equals(array.length, 8192);
                let is_ok = true;
                for (let i = 0, end = array.length-1, asc = 0 <= end; asc ? i <= end : i >= end; asc ? i++ : i--) {
                    if (array[i] !== (10 * i + i)) {
                        is_ok = false;
                        break;
                    }
                }
                assert.ok(is_ok);
                client.exit();
                assert.done();
            });
        });
    });
}

export function getVector(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.getVector( (array) => {
                assert.equals(array.length, 10);
                let is_ok = true;
                for (let i = 0, end = array.length-1, asc = 0 <= end; asc ? i <= end : i >= end; asc ? i++ : i--) {
                    if (array[i] !== (i * i * i)) {
                        is_ok = false;
                        break;
                    }
                }
                assert.ok(is_ok);
                client.exit();
                assert.done();
            });
        });
    });
}

export function getConstVector(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.getConstVector( (array) => {
                assert.equals(array.length, 42);
                let is_ok = true;
                for (let i = 0, end = array.length-1, asc = 0 <= end; asc ? i <= end : i >= end; asc ? i++ : i--) {
                    if (array[i] !== (i * i)) {
                        is_ok = false;
                        break;
                    }
                }
                assert.ok(is_ok);
                client.exit();
                assert.done();
            });
        });
    });
}

export function getConstAutoVector(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.getConstAutoVector( (array) => {
                assert.equals(array.length, 100);
                let is_ok = true;
                for (let i = 0, end = array.length-1, asc = 0 <= end; asc ? i <= end : i >= end; asc ? i++ : i--) {
                    if (array[i] !== (42 * i)) {
                        is_ok = false;
                        break;
                    }
                }
                assert.ok(is_ok);
                client.exit();
                assert.done();
            });
        });
    });
}

export function setString(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.setString( is_ok => {
                assert.ok(is_ok);
                client.exit();
                assert.done();
            });
        });
    });
}

export function getString(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.getString( (str) => {
                assert.equals(str, 'Hello World');
                client.exit();
                assert.done();
            });
        });
    });
}

export function getConstString(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.getConstString( (str) => {
                assert.equals(str, 'Hello World const');
                client.exit();
                assert.done();
            });
        });
    });
}

export function getJSON(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.getJSON( data => {
                assert.ok(data.date != null);
                assert.ok(data.machine != null);
                assert.ok(data.time != null);
                assert.ok(data.user != null);
                assert.ok(data.version != null);
                assert.equal(data.date, '20/07/2016');
                assert.equal(data.machine, 'PC-3');
                assert.equal(data.time, '18:16:13');
                assert.equal(data.user, 'thomas');
                assert.equal(data.version, '0691eed');
                client.exit();
                assert.done();
            });
        });
    });
}

export function getTuple(assert) {
    let client = new Client(HOST, 1);
    assert.doesNotThrow( () => {
        client.init( () => {
            let tests = new Tests(client);
            tests.getTuple( tuple => {
                assert.equals(tuple[0], 501762438);
                assert.ok(Math.abs(tuple[1] - 507.3858) < 5e-6);
                assert.ok(Math.abs(tuple[2] - 926547.6468507200) < 1e-14);
                assert.ok(tuple[3]);
                client.exit();
                assert.done();
            });
        });
    });
}
