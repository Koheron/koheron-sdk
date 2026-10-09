const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const {spawnSync} = require('node:child_process');
const {test} = require('node:test');
const {transpileModule} = require('../transpile.cjs');

test('complete-program checking, ordered globals and unchanged output after type errors', t => {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'koheron-compiler-'));
    t.after(() => fs.rmSync(directory, {recursive: true, force: true}));
    const base = path.join(directory, 'base.ts');
    const child = path.join(directory, 'child.ts');
    const types = path.join(directory, 'types.d.ts');
    const output = path.join(directory, 'app.js');
    fs.writeFileSync(types, 'interface Result {value: number}\n');
    fs.writeFileSync(base, 'class Base { value = 7; constructor() { this.value = 9; } }\n');
    fs.writeFileSync(child, 'class Child extends Base { value: number; }\nconst result: Result = new Child();\n');
    const compile = () => spawnSync(process.execPath, [path.join(__dirname, '../build.cjs'),
        '--output', output, types, base, child], {encoding: 'utf8'});
    const successful = compile();
    assert.equal(successful.status, 0, successful.stdout + successful.stderr);
    const built = fs.readFileSync(output, 'utf8');
    assert.equal(vm.runInNewContext(built + '\nresult.value'), 9);
    fs.writeFileSync(child, 'const result: Result = {value: "invalid"};\n');
    const failed = compile();
    assert.notEqual(failed.status, 0);
    assert.match(failed.stdout + failed.stderr, /not assignable/);
    assert.equal(fs.readFileSync(output, 'utf8'), built);
});

test('compiled FFT worker serializes as a self-contained executable function', () => {
    const source = fs.readFileSync(path.join(__dirname, '../fft/plot/psd-stream.ts'), 'utf8');
    const compiled = transpileModule(source, {compilerOptions: {alwaysStrict: true}}).outputText;
    const worker = vm.runInNewContext(compiled + '\nPSDStream.run.toString()');
    const scope = {};
    vm.runInNewContext(`(${worker})(scope);`, {scope});
    assert.equal(typeof scope.onmessage, 'function');
});
