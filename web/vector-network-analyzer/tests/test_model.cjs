const assert=require('node:assert/strict');
const fs=require('node:fs');const vm=require('node:vm');const test=require('node:test');
const context={console,setTimeout,clearTimeout};vm.runInNewContext(fs.readFileSync('tmp/examples/alpha250/vector-network-analyzer/web/app.js','utf8'),context);
const {VNAModel}=context.VNA;
function frame(gain=1,delay=0,source='hardware') {
  const settings={start:1000,stop:1e6,points:101,duration:.25,level:-6};
  const frequency=Array.from({length:101},(_,i)=>1000*1000**(i/100));
  return {source,settings,timestamp:'2026-10-11T00:00:00Z',frequency,
    real:frequency.map(f=>gain*Math.cos(2*Math.PI*f*delay)),imag:frequency.map(f=>-gain*Math.sin(2*Math.PI*f*delay))};
}
test('duration limits follow chirp coefficient capacity before acquisition',()=>{
  const s={start:1000,stop:100e6,points:501,duration:.02,level:-6};
  assert.throws(()=>VNAModel.validateSettings(s),/longer duration/);
  assert.throws(()=>VNAModel.validateSettings({...s,duration:.05}),/longer duration/);
  assert.equal(VNAModel.validateSettings({...s,duration:.1}).duration,.1);
  assert.equal(VNAModel.validateSettings({...s,start:1e6,stop:10e6}).duration,.02);
});
test('automatic records include response tail and use their own FFT grid',()=>{
  const s={start:1000,stop:100e6,points:2001,duration:.1,level:-6,record:'auto'};
  assert.equal(VNAModel.recordSamples(s),32*1024*1024);
  assert.equal(VNAModel.recordSamples({...s,duration:.25}),64*1024*1024);
  assert.equal(VNAModel.recordSamples({...s,record:'full'}),64*1024*1024);
  assert.equal(VNAModel.recordSamples({...s,record:undefined}),64*1024*1024);
  assert.ok(VNAModel.recordSamples(s)/VNAModel.FS-s.duration >= .010);
  const f=frame();f.settings={...s};f.recordSamples=VNAModel.recordSamples(s);
  f.frequency=Array.from(VNAModel.frequencyBins(s),k=>k*VNAModel.FS/f.recordSamples);
  f.real=f.frequency.map(()=>1);f.imag=f.frequency.map(()=>0);
  assert.equal(VNAModel.validateFrame(f).recordSamples,f.recordSamples);
  assert.throws(()=>VNAModel.validateFrame({...f,recordSamples:64*1024*1024}),/record length/);
  const model=new VNAModel();model.settings=s;model.calibration=f;model.latest=f;
  assert.equal(model.calibrationMatches(),true);
  model.settings={...s,record:'full'};assert.equal(model.calibrationMatches(),false);
  const restored=new VNAModel();restored.load(model.serialize());
  assert.equal(restored.calibration.recordSamples,32*1024*1024);
});
test('complex thru calibration removes fixture gain and delay',()=>{
  const model=new VNAModel();model.calibration=frame(.7,8e-9);model.latest=frame(.35,23e-9);
  const d=model.display(model.latest);assert.equal(d.calibrated,true);
  d.magnitude.forEach(v=>assert.ok(Math.abs(v-20*Math.log10(.5))<1e-10));
  d.delay.forEach(v=>assert.ok(Math.abs(v-15)<1e-10));
});
test('calibration rejects mismatched sweep settings, grids, synthetic data and weak bins',()=>{
  const model=new VNAModel();model.calibration=frame();
  for(const changed of [f=>f.settings.level=-3,f=>f.frequency[3]+=.1,f=>f.source='demo']) {
    const f=frame();changed(f);assert.equal(model.display(f).calibrated,false);
  }
  const f=frame();model.calibration.real[4]=0;model.calibration.imag[4]=0;
  assert.equal(model.display(f).frame.real[4],null);
});
test('saved calibrated traces do not receive calibration or port extension twice',()=>{
  const model=new VNAModel();model.calibration=frame(.5,10e-9);model.latest=frame(.25,30e-9);model.delayNs=7;
  model.captureReference('Filter');const saved=model.display(model.references[0].frame,false);
  assert.ok(Math.abs(saved.delay[40]-13)<1e-10);
  assert.ok(Math.abs(saved.magnitude[40]-20*Math.log10(.5))<1e-10);
});
test('session load is atomic and round trips complex values and captured settings',()=>{
  const model=new VNAModel();model.latest=frame();model.settings={...model.latest.settings};model.captureReference();
  const serialized=model.serialize(),other=new VNAModel();other.load(serialized);assert.deepEqual(JSON.parse(other.serialize()),JSON.parse(serialized));
  const invalid=JSON.parse(serialized);invalid.references[0].frame.frequency[3]=NaN;
  const before=other.serialize();assert.throws(()=>other.load(JSON.stringify(invalid)));assert.equal(other.serialize(),before);
});
test('CSV identifies correction, source and extension and exports gaps without NaN',()=>{
  const model=new VNAModel();model.latest=frame();model.latest.real[3]=model.latest.imag[3]=null;
  const csv=model.csv();assert.match(csv,/source: hardware/);assert.match(csv,/frequency_Hz,real,imaginary/);assert.doesNotMatch(csv,/NaN/);
});
test('phase unwrap resets at invalid bins and preserves known group delay',()=>{
  const model=new VNAModel();model.latest=frame(.5,700e-9);model.unwrap=true;
  const d=model.display(model.latest);assert.ok(Math.abs(d.delay[20]-700)<1e-8);
  model.latest.real[50]=model.latest.imag[50]=null;
  const gaps=model.display(model.latest);assert.ok(Number.isNaN(gaps.delay[49]));assert.ok(Number.isNaN(gaps.delay[50]));
});
test('shared references enforce eight captures and undo recapture without losing edits',()=>{
  const model=new VNAModel();model.latest=frame(.5,10e-9);model.captureReference('Filter');
  const collection=model.referenceCollection,item=collection.items[0];
  model.latest=frame(.25,30e-9);model.replaceReference(item);
  collection.items[0].name='Renamed';collection.items[0].visible=false;
  collection.undo();assert.equal(collection.items[0].name,'Renamed');assert.equal(collection.items[0].visible,false);
  assert.ok(Math.abs(model.display(collection.items[0].frame,false).delay[40]-10)<1e-10);
  for(let i=1;i<8;i++) model.captureReference();
  assert.throws(()=>model.captureReference(),/8 references/);
  collection.remove(collection.items[3]);assert.equal(collection.items.length,7);
  collection.undo();assert.equal(collection.items.length,8);
});
test('reference import is atomic and keeps independent grids, correction and source',()=>{
  const model=new VNAModel('hardware');model.latest=frame();model.captureReference('Original');
  const file=JSON.parse(model.referenceCollection.serialize());
  file.references[0].frame.frequency[3]=0;
  const before=model.referenceCollection.serialize();
  assert.throws(()=>model.referenceCollection.load(JSON.stringify(file)));
  assert.equal(model.referenceCollection.serialize(),before);
  file.references[0].frame=frame(1,0,'demo');
  assert.throws(()=>model.referenceCollection.load(JSON.stringify(file)),/kept separate/);
  file.references[0].frame=frame(.2,20e-9);file.references[0].calibrated=true;
  model.referenceCollection.load(JSON.stringify(file));
  assert.equal(model.references[1].name,'Original (2)');assert.equal(model.references[1].calibrated,true);
  assert.notEqual(model.references[0].color,model.references[1].color);
});
test('earlier V1 sessions retain their four saved traces and correction state',()=>{
  const model=new VNAModel();model.latest=frame();model.captureReference('Legacy');
  const file=JSON.parse(model.serialize());delete file.references[0].capturedAt;file.references[0].id=1;
  const next=new VNAModel();next.load(JSON.stringify(file));
  assert.equal(next.references[0].name,'Legacy');assert.equal(next.references[0].capturedAt,file.latest.timestamp);
  assert.deepEqual(JSON.parse(JSON.stringify(next.references[0].frame)),file.references[0].frame);
});
test('session restore preserves intentionally repeated reference names and colors',()=>{
  const model=new VNAModel();model.latest=frame();model.captureReference('Filter');model.captureReference('Filter');
  model.references[1].color=model.references[0].color;
  const serialized=model.serialize(),next=new VNAModel();next.load(serialized);
  assert.deepEqual(JSON.parse(next.serialize()),JSON.parse(serialized));assert.equal(next.referenceCollection.undoLabel,'');
});
