// Compare ADC processing paths with identical records in the same Wasm instance.
const fs = require('node:fs');
const {performance} = require('node:perf_hooks');

(async () => {
  const path = process.argv[2] || 'tmp/examples/alpha250/vector-network-analyzer/chirp-processor.wasm';
  const p = (await WebAssembly.instantiate(fs.readFileSync(path))).instance.exports;
  for (const n of [16,32,64].map(m => m*1048576)) {
    const bytes = p.layout(n), extra = Math.ceil((bytes-p.memory.buffer.byteLength)/65536);
    if (extra>0) p.memory.grow(extra);
    const raw = new Int16Array(p.memory.buffer,p.adc_address(),n);
    raw.fill(0); raw[13] = 24576;
    const results = {paired:[], blocked:[], fused:[]};
    for (let trial=0;trial<3;++trial) {
      for (const mode of ['paired','blocked','fused']) {
        const start = performance.now();
        if (mode === 'fused') p.adc_fft_block(0,n/2);
        else { p.adc_block(0,n); p.reorder_block(0,n/2); }
        const prefix = mode === 'paired' ? 1 : p.fft_prefix_length();
        if (prefix > 1) p.fft_prefix_block(0,n/2,mode === 'fused' ? 1 : 0);
        for (let length=prefix*2;length<=n/2;) {
          const pair = length*2<=n/2, last = pair?length*2:length;
          const angle = -2*Math.PI/last;
          if (pair) p.fft_stage_pair(last,Math.cos(angle),Math.sin(angle));
          else p.fft_stage(last,Math.cos(angle),Math.sin(angle));
          length=last*2;
        }
        results[mode].push(performance.now()-start);
        for (const k of [269, Math.floor(n*.4), n/2-19]) {
          const a=2*Math.PI*k/n, phase=13*a;
          const real=p.bin_real(k,Math.cos(a),Math.sin(a));
          const imag=p.bin_imag(k,Math.cos(a),Math.sin(a));
          if (Math.hypot(real-.75*Math.cos(phase),imag+.75*Math.sin(phase))>5e-6)
            throw new Error(`${mode}: incorrect impulse response at ${k}`);
        }
      }
    }
    const median = values => values.sort((a,b)=>a-b)[1];
    const paired = median(results.paired), blocked = median(results.blocked), fused = median(results.fused);
    console.log(JSON.stringify({samples:n,pairedMs:paired,blockedMs:blocked,fusedMs:fused,speedup:paired/fused}));
  }
})().catch(error => {console.error(error);process.exitCode=1;});
