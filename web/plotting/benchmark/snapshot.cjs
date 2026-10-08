// Save the currently built owned renderer/widget before changing an optimization.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {root, scripts} = require('./harness.cjs');
const dest = path.join(root, 'tmp/plotting/comparison');
fs.mkdirSync(dest, {recursive: true});
const sha256 = {};
for (const name of [...scripts, 'plot-basics.ts']) {
    const source = name === 'plot-basics.ts' ? path.join(root, 'web/plot-basics', name) : path.join(root, 'tmp/plotting/owned', name);
    const bytes = fs.readFileSync(source);
    fs.writeFileSync(path.join(dest, name), bytes);
    sha256[name] = crypto.createHash('sha256').update(bytes).digest('hex');
}
fs.writeFileSync(path.join(dest, 'snapshot.json'), JSON.stringify({created: new Date().toISOString(), sha256}, null, 2) + '\n');
console.log(`Saved previous variant: ${dest}`);
