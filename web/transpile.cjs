// One emitter for production scripts and browser-test fixtures. TypeScript's
// native compiler checks the complete source set separately in build.cjs.
const {transformSync} = require('esbuild');

function transpileModule(source, options = {}) {
    const target = options.compilerOptions?.target || 'es2020';
    const result = transformSync(source, {
        loader: 'ts',
        target,
        tsconfigRaw: {compilerOptions: {
            alwaysStrict: options.compilerOptions?.alwaysStrict || false,
            useDefineForClassFields: false,
        }},
    });
    return {outputText: result.code};
}

module.exports = {transpileModule, ScriptTarget: {ES2020: 'es2020'}};
