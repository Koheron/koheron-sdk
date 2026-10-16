"""Exercise the actual webpage's binary RPC, quiet wait and control paths."""

from pathlib import Path
import shutil
import subprocess

import pytest


def test_quiet_wait_controls_and_stale_refresh():
    if not shutil.which("node"):
        pytest.skip("Node is required for the browser protocol check")
    script = Path(__file__).parents[1] / "web/full.js"
    runner = r"""
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const nodes=new Map(),calls=[],timers=new Map();let serial=0,active=false,finish,mainSocket;
const node=()=>({textContent:'',replaceChildren(){},append(){}});
const context=[
 {class:'BistFull',id:7,functions:[{name:'start',id:1,ret_type:'bool'},{name:'cancel',id:2,ret_type:'void'},{name:'get_report',id:3,ret_type:'std::string'}]},
 {class:'FullCompletion',id:8,functions:[{name:'wait',id:1,ret_type:'bool'}]}
];
class WebSocket{
 constructor(url){this.url=url;mainSocket ||= this;queueMicrotask(()=>this.onopen());}
 send(packet){
  const v=new DataView(packet),driver=v.getUint16(4),id=v.getUint16(6);
  const reply=value=>{
   const text=typeof value==='boolean'?null:new TextEncoder().encode(JSON.stringify(value));
   const data=new ArrayBuffer(text?12+text.length:9),out=new DataView(data);
   if(text){out.setUint32(8,text.length);new Uint8Array(data,12).set(text);}else out.setUint8(8,Number(value));
   queueMicrotask(()=>this.onmessage({data}));
  };
  if(driver===1){calls.push('context');reply(context);}
  else if(driver===8){assert.strictEqual(v.getUint32(8),60000);calls.push('wait');if(active)finish=()=>reply(true);else reply(true);}
  else if(driver===7&&id===1){
   calls.push('start');assert.deepStrictEqual([v.getUint32(8),v.getUint32(12),v.getUint32(16)],[1,35000,0xb157f011]);active=true;reply(true);
  }else if(driver===7&&id===2){calls.push('cancel');active=false;}
  else if(driver===7&&id===3){calls.push('report');reply({run_id:'1',state:'complete',observer_requests:0,checks:[],metadata:{}});}
  else throw new Error('unexpected command');
 }
 close(){}
}
const browser={WebSocket,ArrayBuffer,DataView,Uint8Array,TextDecoder,
 location:{protocol:'http:',hostname:'board'},
 document:{getElementById(id){if(!nodes.has(id))nodes.set(id,node());return nodes.get(id);},createElement:node},
 setTimeout(fn,ms){const id=++serial;timers.set(id,{fn,ms});return id;},clearTimeout(id){timers.delete(id);}};
const flush=async()=>{for(let i=0;i<30;++i)await Promise.resolve();};
(async()=>{
 vm.runInNewContext(fs.readFileSync(process.argv[1],'utf8'),browser);await flush();
 assert.deepStrictEqual(calls,['context','wait','report']);
 browser.document.getElementById('mask').value='1';await nodes.get('start').onclick();await flush();
 assert.strictEqual(nodes.get('cancel').disabled,false);
 const refresh=[...timers.values()].find(t=>t.ms===500).fn;refresh();await flush();
 assert.strictEqual(calls.at(-1),'wait');const before=calls.length;
 await flush();assert.strictEqual(calls.length,before); // No reports during measurement.
 assert([...timers.values()].some(t=>t.ms===65000));
 nodes.get('cancel').onclick();await flush();assert.strictEqual(calls.at(-1),'cancel');
 finish();await flush();assert.strictEqual(calls.at(-1),'report');
 mainSocket.onclose();const after=calls.length;refresh();await flush();assert.strictEqual(calls.length,after);
})().catch(error=>{console.error(error);process.exitCode=1;});
"""
    subprocess.run(["node", "-e", runner, str(script)], check=True, timeout=10)
