const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const test = require('node:test');
const {execFileSync} = require('node:child_process');
const root = path.resolve(__dirname, '../../..');
const manifest = require('../upstream.json');
test('frozen original benchmark has the recorded upstream bytes', () => {
    for (const [name, hash] of Object.entries(manifest.baselineSHA256)) {
        const bytes = fs.readFileSync(path.join(__dirname, '../benchmark/baseline', name));
        assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'), hash, name);
    }
});
test('local builds are deterministic, retain license/attribution and need no downloads', () => {
    const out = path.join(root, 'tmp/plotting/asset-test');
    execFileSync(process.execPath, [path.join(__dirname, '../build.cjs'), out]);
    const first = new Map(fs.readdirSync(out).map(name => [name, fs.readFileSync(path.join(out,name))]));
    execFileSync(process.execPath, [path.join(__dirname, '../build.cjs'), out]);
    for (const [name,bytes] of first) {
        assert.deepEqual(fs.readFileSync(path.join(out,name)), bytes);
        assert.match(bytes.toString(), /Permission is hereby granted/);
        if (name.endsWith('.js')) {
            assert.match(bytes.toString(), /Koheron owned Flot/);
            assert.doesNotThrow(() => new (require('node:vm').Script)(bytes.toString()));
        }
    }
    assert.match(first.get('jquery.flot.axislabels.js').toString(), /Xuan Luo/);
    assert.match(first.get('jquery.flot.js').toString(), /IOLA and Ole Laursen/);
    assert.match(first.get('jquery.flot.js').toString(), /Ole Laursen, October 2009/);
});
