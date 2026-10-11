const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const test = require('node:test');
function model(board) {
    const context = {console, setTimeout, clearTimeout};
    vm.runInNewContext(fs.readFileSync(`tmp/examples/${board}/vector-network-analyzer/web/app.js`, 'utf8'), context);
    return context.VNA.VNAModel;
}
test('Red Pitaya selects its clock, source quantization and valid sweep limits', () => {
    const Model = model('red-pitaya'), instance = new Model();
    assert.equal(Model.FS, 125e6);
    assert.equal(Model.SOURCE_SHIFT, 2);
    assert.equal(instance.settings.stop, 50e6);
    assert.equal(Model.validateSettings({...instance.settings, duration: .1}).duration, .1);
    assert.equal(Model.recordSamples({...instance.settings, duration:.1}),16*1024*1024);
    assert.equal(Model.recordSamples(instance.settings),32*1024*1024);
    assert.equal(Model.recordSamples({...instance.settings,record:'full'}),64*1024*1024);
    assert.throws(() => Model.validateSettings({...instance.settings, duration: .05}), /longer duration/);
    assert.throws(() => Model.validateSettings({...instance.settings, stop: 100e6}), /50 MHz/);
    assert.equal(Model.validateSettings({...instance.settings, start: 1e6, stop: 10e6, duration: .05}).duration, .05);
});
test('Board-specific sessions and references cannot mix calibration between converters', () => {
    const Alpha = model('alpha250'), Red = model('red-pitaya');
    const alpha = new Alpha(), red = new Red();
    const before = red.serialize();
    assert.throws(() => red.load(alpha.serialize()));
    assert.equal(red.serialize(), before);
    assert.throws(() => red.referenceCollection.load(alpha.referenceCollection.serialize()));
    assert.equal(JSON.parse(red.serialize()).format, 'koheron-red-pitaya-vna');
    const restored = new Red(); restored.load(red.serialize());
    assert.equal(restored.serialize(), red.serialize());
    assert.equal(Alpha.FS, 250e6);
    assert.equal(Alpha.SOURCE_SHIFT, 0);
});
