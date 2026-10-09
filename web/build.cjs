// Check the full global TypeScript program, then emit one ordered browser script.
const fs = require('node:fs');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {transpileModule} = require('./transpile.cjs');

function build(output, files) {
    const sources = [...new Set(files.map(file => path.resolve(file)))];
    if (!sources.length || sources.some(file => !file.endsWith('.ts'))) {
        throw new Error('Expected TypeScript source files');
    }
    const typescript = path.dirname(require.resolve('typescript/package.json'));
    const result = spawnSync(process.execPath, [path.join(typescript, 'bin/tsc'),
        '--ignoreConfig', '--noEmit', '--strict', 'false', '--alwaysStrict',
        '--skipLibCheck', '--target', 'es2020', '--lib', 'es2020,dom',
        '--module', 'preserve', '--moduleDetection', 'legacy',
        '--typeRoots', path.join(typescript, '../@types'),
        '--types', 'jquery,jquery-mousewheel,node', ...sources,
    ], {stdio: 'inherit'});
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(`TypeScript check failed (${result.status})`);

    const source = sources.filter(file => !file.endsWith('.d.ts'))
        .map(file => `// ${path.basename(file)}\n${fs.readFileSync(file, 'utf8')}\n`)
        .join('\n');
    const {outputText} = transpileModule(source, {compilerOptions: {alwaysStrict: true}});
    fs.mkdirSync(path.dirname(output), {recursive: true});
    const temporary = `${output}.${process.pid}.tmp`;
    try {
        fs.writeFileSync(temporary, outputText);
        fs.renameSync(temporary, output);
    } finally {
        fs.rmSync(temporary, {force: true});
    }
}

if (require.main === module) {
    const [flag, output, ...files] = process.argv.slice(2);
    if (flag !== '--output' || !output) {
        console.error('Usage: node build.cjs --output OUTPUT SOURCE.ts [...]');
        process.exitCode = 2;
    } else {
        try { build(output, files); }
        catch (error) { console.error(error.message); process.exitCode = 1; }
    }
}

module.exports = {build};
