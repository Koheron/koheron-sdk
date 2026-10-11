// Local protocol fixture, using production metadata and exact chirp records.
// This is synthetic hardware input, never a substitute for board validation.
const fs=require('node:fs');const http=require('node:http');
const {WebSocketServer}=require('ws');
const red=process.argv.includes('--red-pitaya'),rate=red?125e6:250e6;
const base=`tmp/examples/${red?'red-pitaya':'alpha250'}/vector-network-analyzer`;
(async()=>{
  const metadata=JSON.parse(fs.readFileSync(`${base}/server/drivers.json`));
  const driver=metadata.find(d=>d.class==='VectorNetworkAnalyzer');
  const names=new Map(driver.functions.map(c=>[c.id,c.name]));
  const plans=['','-short','-narrow'].map(suffix=>JSON.parse(fs.readFileSync(`${base}/test-plan${suffix}.json`)));
  const p=(await WebAssembly.instantiate(fs.readFileSync(`${base}/chirp-processor.wasm`))).instance.exports;
  let n=0,raw,plan,sequence=0,owned=0,fault=0,blocks=0,batches=0,releases=0,pending=0,releasePending=0;
  function record(samples, duration, start, stop, level) {
    const near=(a,b)=>Math.abs(a-b)<.01;
    const choice=near(start,1000000)&&near(stop,5500000)&&duration===.02?2:near(start,500)&&near(stop,rate*.44)?(duration===.25?0:duration===.1?1:-1):-1;
    if(choice<0)throw new Error('Unsupported fixture sweep');
    plan=plans[choice].slice(); plan[1]=Math.min(32767,Math.round(32768*10**(level/20)));
    n=samples; const bytes=p.layout(n);
    const extra=Math.ceil((bytes-p.memory.buffer.byteLength)/65536); if(extra>0)p.memory.grow(extra);
    const lut=new Int16Array(p.memory.buffer,p.lut_address(),65536);
    for(let i=0;i<65536;i++)lut[i]=Math.round(32766*Math.sin(2*Math.PI*i/65536));
    p.set_source_shift(red?2:0);p.set_coefficient(plan[2],plan[3]);
    for(let i=0;i<16;i++)p.seed(i,plan[4+2*i],plan[5+2*i]);
    p.reference_block(0,n,plan[0],plan[1]);
    const work=new Float32Array(p.memory.buffer,p.work_address(),n),adc=new Int16Array(p.memory.buffer,p.adc_address(),n);
    adc.fill(0);for(let i=11;i<n;i++)adc[i]=Math.round(work[i-11]*16384);
    raw=Buffer.from(p.memory.buffer,p.adc_address(),2*n);
  }
  function reply(socket,header,data,dynamic=false){
    const buffer=Buffer.alloc((dynamic?12:8)+data.length);header.copy(buffer,0,0,8);
    if(dynamic)buffer.writeUInt32BE(data.length,8);data.copy(buffer,dynamic?12:8);socket.send(buffer);
  }
  const server=new WebSocketServer({host:'127.0.0.1',port:8080});
  server.on('connection',socket=>socket.on('message',request=>{
    const header=request.subarray(0,8),classId=request.readUInt16BE(4),id=request.readUInt16BE(6);
    if(classId===1&&id===1){reply(socket,header,Buffer.from(JSON.stringify(metadata)),true);return;}
    if(classId!==driver.id)throw new Error('Unexpected driver');
    const name=names.get(id);
    if(name==='begin_sweep'||name==='begin_recorded_sweep'){
      const out=Buffer.alloc(8);if(owned)out.writeUInt32BE(2,4);else{record(name==='begin_sweep'?67108864:request.readUInt32BE(40),request.readDoubleBE(24),request.readDoubleBE(8),request.readDoubleBE(16),request.readDoubleBE(32));owned=++sequence;out.writeUInt32BE(owned,0);blocks=0;batches=0;}
      reply(socket,header,out);return;
    }
    const token=request.readUInt32BE(8);
    if(name==='get_plan'){
      const out=Buffer.alloc(144);plan.forEach((v,i)=>out.writeUInt32LE(token===owned?v:0,4*i));reply(socket,header,out);
    }else if(name==='get_status'){
      const out=Buffer.alloc(4);out.writeUInt32BE(token===owned?(fault===9?0:([3,4,5,6,7].includes(fault)?fault:1)):5);reply(socket,header,out);
    }else if(name==='get_record_samples'){
      const out=Buffer.alloc(4);out.writeUInt32BE(token===owned?(fault===11?n/2:n):0);reply(socket,header,out);
    }else if(name==='get_adc_block'||name==='get_adc_blocks'){
      const index=request.readUInt32BE(12),count=name==='get_adc_blocks'?request.readUInt32BE(16):1;blocks+=count;batches++;
      const data=token===owned&&count>0&&count<=16&&index+count<=n*2/262144?raw.subarray(index*262144,(index+count)*262144-(fault===10?4:0)):Buffer.alloc(0);
      pending++;
      const send=()=>{pending--;reply(socket,header,data,true);};
      // Delay replies so cancellation must finish the pending read before
      // releasing acquisition ownership.
      if(fault===13)setTimeout(send,500);else send();
    }else if(name==='release'){
      const accepted=!!owned&&token===owned;if(accepted){releasePending+=pending;owned=0;releases++;}reply(socket,header,Buffer.from([accepted?1:0]));
    }else throw new Error(`Unexpected RPC ${name}`);
  }));
  http.createServer((request,response)=>{
    const url=new URL(request.url,'http://127.0.0.1');if(url.searchParams.has('fault'))fault=Number(url.searchParams.get('fault'));
    response.setHeader('Content-Type','application/json');response.end(JSON.stringify({owned,sequence,fault,blocks,batches,releases,pending,releasePending}));
  }).listen(8090,'127.0.0.1');
  console.log('VNA protocol emulator ready on 127.0.0.1:8080');
})().catch(error=>{console.error(error);process.exitCode=1;});
