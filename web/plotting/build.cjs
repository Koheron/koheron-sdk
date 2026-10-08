// Build the owned sources; no plotting code is fetched at build time.
const fs = require('node:fs');
const path = require('node:path');
let terser;
try { terser = require('terser'); } catch (_) { terser = require('/opt/app/plotting/node_modules/terser'); }
const files = ['jquery.flot.js', 'jquery.flot.resize.js', 'jquery.flot.selection.js', 'jquery.flot.time.js', 'jquery.flot.axislabels.js', 'jquery.flot.canvas.js'];
(async () => {
    const out = path.resolve(process.argv[2] || path.join(__dirname, '../../tmp/plotting/owned'));
    fs.mkdirSync(out, {recursive: true});
    for (const file of files) {
        let code = fs.readFileSync(path.join(__dirname, 'src', file), 'utf8');
        if (file === 'jquery.flot.js') code = fs.readFileSync(path.join(__dirname, 'src/jquery.colorhelpers.js'), 'utf8') + '\n' + code;
        const license = file === 'jquery.flot.axislabels.js' ? code.slice(0, code.indexOf('*/') + 2)
            : '/*\n' + fs.readFileSync(path.join(__dirname, 'licenses/LICENSE.txt'), 'utf8') + '\n*/';
        const result = await terser.minify({[file]: code}, {ecma: 5, compress: true, mangle: true, keep_fnames: true,
            format: {comments: /Copyright|Licensed|license|Inline dependency/}});
        fs.writeFileSync(path.join(out, file), license + '\n/* Koheron owned Flot 0.8.3; see web/plotting/README.md. */\n' + result.code + '\n');
    }
    // Include the upstream MIT license with each packaged instrument as well.
    fs.copyFileSync(path.join(__dirname, 'licenses/LICENSE.txt'), path.join(out, 'flot-LICENSE.txt'));
    console.log(`Built ${files.length} plotting assets in ${out}`);
})().catch(e => { console.error(e); process.exitCode = 1; });
