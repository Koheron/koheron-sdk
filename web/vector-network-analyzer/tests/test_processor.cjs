const assert = require('node:assert/strict');
const fs = require('node:fs');
const test = require('node:test');
const wasm = 'tmp/examples/alpha250/vector-network-analyzer/chirp-processor.wasm';
async function processor(n) {
  const p = (await WebAssembly.instantiate(fs.readFileSync(wasm))).instance.exports;
  const bytes = p.layout(n); assert.ok(bytes);
  p.memory.grow(Math.max(0, Math.ceil((bytes - p.memory.buffer.byteLength) / 65536)));
  return p;
}
function fft(p, n, adc=false) {
  if(adc)p.adc_fft_block(0,n/2);else p.reorder_block(0, n/2);
  const prefix=p.fft_prefix_length();p.fft_prefix_block(0,n/2,adc?1:0);
  for (let length=prefix*2; length<=n/2;) {
    const paired=length*2<=n/2,last=paired?length*2:length,a=-2*Math.PI/last;
    if(paired)p.fft_stage_pair(last,Math.cos(a),Math.sin(a));
    else p.fft_stage(last,Math.cos(a),Math.sin(a));
    length=last*2;
  }
}
function bin(p,n,k) { const a=2*Math.PI*k/n; return [p.bin_real(k,Math.cos(a),Math.sin(a)),p.bin_imag(k,Math.cos(a),Math.sin(a))]; }

test('packed real FFT matches an independent DFT including DC and Nyquist', async () => {
  for (const n of [32,64,256,1024]) {
    const p=await processor(n), w=new Float32Array(p.memory.buffer,p.work_address(),n);
    const input=Array.from({length:n},(_,i)=>Math.sin(i*.743)*.3+Math.cos(i*.117)*.2+.1);
    w.set(input); fft(p,n);
    for (let k=0;k<=n/2;k++) {
      const expected=input.reduce((out,v,i)=>[out[0]+v*Math.cos(2*Math.PI*k*i/n),out[1]-v*Math.sin(2*Math.PI*k*i/n)],[0,0]);
      const actual=bin(p,n,k);
      assert.ok(Math.hypot(actual[0]-expected[0],actual[1]-expected[1])<2e-5,`N=${n}, bin=${k}: ${actual} versus ${expected}`);
    }
  }
});

test('fused ADC conversion and early stages match an independent DFT', async () => {
  for(const n of [32,64,256,1024]) {
    const p=await processor(n),raw=new Int16Array(p.memory.buffer,p.adc_address(),n);
    raw.set(Array.from({length:n},(_,i)=>Math.round(17000*Math.sin(i*.743)+9000*Math.cos(i*.117))));
    const input=Array.from(raw,v=>v/32768);fft(p,n,true);
    for(let k=0;k<=n/2;k++) {
      const expected=input.reduce((out,v,i)=>[out[0]+v*Math.cos(2*Math.PI*k*i/n),out[1]-v*Math.sin(2*Math.PI*k*i/n)],[0,0]);
      const actual=bin(p,n,k);
      assert.ok(Math.hypot(actual[0]-expected[0],actual[1]-expected[1])<4e-5,`N=${n}, bin=${k}: ${actual} versus ${expected}`);
    }
    assert.deepEqual(Array.from(raw),input.map(v=>v*32768),'ADC record stays immutable');
  }
});

for (const shift of [0,2]) test(`reference arithmetic and ${16-shift}-bit source match exact BigInt arithmetic`, async () => {
  const n=65536, p=await processor(n), lut=new Int16Array(p.memory.buffer,p.lut_address(),65536);
  for(let i=0;i<65536;i++) lut[i]=Math.round(32766*Math.sin(2*Math.PI*i/65536));
  const c=73129485926001n, states=Array.from({length:16},(_,i)=>1234567891234567n+BigInt(i)*1293891234567n);
  const original=states.slice(); p.set_coefficient(Number(c&0xffffffffn),Number(c>>32n));
  states.forEach((v,i)=>p.seed(i,Number(v&0xffffffffn),Number(v>>32n)));
  const samples=59013, gain=16423; p.set_source_shift(shift);
  for(let first=0;first<n;first+=997) p.reference_block(first,997,samples,gain);
  const w=new Float32Array(p.memory.buffer,p.work_address(),n); let phase=0n;
  for(let i=0;i<n;i++) {
    let expected=0;
    if(i<samples) {
      const lane=i&15, state=states[lane]; phase=(phase+((state+32768n)>>16n))&((1n<<48n)-1n);
      states[lane]=state+((state*c+(1n<<63n))>>64n);
      const taper=BigInt(Math.min(i,samples-1-i,262144));
      const tapered=(BigInt(lut[Number(phase>>32n)])*taper)>>18n;
      const output=(tapered*BigInt(gain))>>15n;
      expected=Number((output>>BigInt(shift))<<BigInt(shift))/32768;
    }
    assert.equal(w[i],expected,`sample ${i}`);
  }
  assert.notEqual(original[0],states[0]);
});

test('FFT ratio recovers a known FIR and delay without record windowing', async () => {
  const n=8192,p=await processor(n),work=new Float32Array(p.memory.buffer,p.work_address(),n);
  const x=Array.from({length:n},(_,i)=>i<n-32?Math.sin(i*1.331)*.04+Math.cos(i*.717)*.05:0);
  work.set(x);fft(p,n);const bins=[1,17,77,301,1111,3000],reference=bins.map(k=>bin(p,n,k));
  work.set(x.map((_,i)=>.2*(x[i-7]||0)+.4*(x[i-8]||0)+.2*(x[i-9]||0)));fft(p,n);
  bins.forEach((k,i)=>{
    const y=bin(p,n,k),a=reference[i],power=a[0]*a[0]+a[1]*a[1];
    const actual=[(y[0]*a[0]+y[1]*a[1])/power,(y[1]*a[0]-y[0]*a[1])/power];
    const expected=[0,0];[.2,.4,.2].forEach((v,j)=>{expected[0]+=v*Math.cos(2*Math.PI*k*(j+7)/n);expected[1]-=v*Math.sin(2*Math.PI*k*(j+7)/n);});
    assert.ok(Math.hypot(actual[0]-expected[0],actual[1]-expected[1])<2e-4);
  });
});

test('full 64 Mi-sample transform retains impulse phase at upper-band bins', async () => {
  const n=67108864,p=await processor(n),work=new Float32Array(p.memory.buffer,p.work_address(),n);
  work[13]=.75;const started=Date.now();fft(p,n);
  for(const k of [269,2684355,26843545]) {
    const actual=bin(p,n,k),a=2*Math.PI*k*13/n;
    assert.ok(Math.hypot(actual[0]-.75*Math.cos(a),actual[1]+.75*Math.sin(a))<5e-6);
  }
  console.log(`Full-record FFT: ${((Date.now()-started)/1000).toFixed(2)} s; workspace ${(p.memory.buffer.byteLength/2**20).toFixed(1)} MiB`);
});
