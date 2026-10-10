// An offline preview of the real FFT workspace with a simulated spectrum.
const fs = require('node:fs');
const path = require('node:path');
const {JSDOM} = require('jsdom');
const {root,scripts,ts} = require('./harness.cjs');
function previewHTML() {
    const read = file => fs.readFileSync(path.join(root,file),'utf8');
    const dom = new JSDOM(read('web/fft/workspace.html'));
    const document = dom.window.document, content = document.querySelector('template').content;
    const mount = (id,file) => {
        const fragment = new JSDOM(read(file));
        content.getElementById(id).append(fragment.window.document.querySelector('template').content.cloneNode(true));
        fragment.window.close();
    };
    mount('yunit','web/fft/plot/yunit.html');
    mount('peak-detection','web/fft/plot/peak-detection.html');
    mount('plot-basics','web/plot-basics/plot-basics.html');
    mount('export-file','web/fft/export-file/export-file.html');
    content.querySelector('.brand').remove(); content.querySelector('nav').remove();
    content.querySelector('h1').textContent='FFT · simulated spectrum';
    content.getElementById('board-label').textContent='Offline preview';
    content.getElementById('instrument-controls').remove();
    content.querySelector('aside').innerHTML='<div id="plot-references"></div>';
    const css = ['web/instrument/instrument.css','web/fft/fft.css','web/plot-references/plot-references.css']
        .filter(file => fs.existsSync(path.join(root,file))).map(read).join('\n');
    const js = [read('web/plotting/benchmark/baseline/jquery.min.js'),
        ...scripts.map(name => read('tmp/plotting/owned/'+name)),
        ts.transpileModule(['web/plot-basics/plot-basics.ts','web/fft/plot/spectrum-history.ts','web/fft/plot/spectrum-views.ts',
            'web/plot-references/references.ts','web/plot-references/panel.ts','web/fft/plot/references.ts',
            'web/fft/plot/plot.ts','web/fft/export-file/export-file.ts'].map(read).join('\n'),
            {compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText];
    const fragment = document.createElement('div'); fragment.append(content.cloneNode(true));
    const html = `<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>FFT persistence preview</title><style>${css}</style></head><body class="instrument-workspace fft-workspace" data-board="simulation">${fragment.innerHTML}
${js.map(source => '<script>'+source.replace(/<\/script/gi,'<\\/script')+'</script>').join('\n')}
<script>
let seed=123456789, frame=0;
const random=()=>{seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return (seed>>>0)/4294967296;};
const driver={fft_size:8192,status:{fs:250e6,W1:.25,W2:.375,channel:0,window_index:1,clkIndex:'1',dds_freq:[0,0]},
    async readSpectrum(){
        frame++;
        const psd=Float32Array.from({length:4096},(_,i)=>{
            const floor=-140+12*random()+3*Math.sin(i*.01+frame*.09);
            const moving=1050+350*Math.sin(frame*.026);
            const tone=-83-18*Math.pow(Math.min(5,Math.abs(i-moving)/4),2);
            const burst=frame%180<15 && Math.abs(i-2900)<8 ? -72 : -200;
            return 1e-3*Math.pow(10,Math.max(floor,tone,burst)/10);
        });
        return {psd,status:this.status};
    }};
const basics=new PlotBasics(document,$('#plot-placeholder'),4096,0,125,-180,-60,driver,'','Frequency (MHz)');
window.spectrum=new Plot(document,driver,basics); window.basics=basics;
const panel=new FFTReferencePanel(document,spectrum.references,()=>spectrum.refreshReferences(),item=>spectrum.replaceReference(item));
new ExportFile(document,spectrum,'Simulation','FFT');
const pause=document.getElementById('pause-display'); pause.disabled=false;
pause.addEventListener('click',()=>{const paused=pause.getAttribute('aria-pressed')!=='true';pause.setAttribute('aria-pressed',String(paused));pause.textContent=paused?'Resume':'Pause';spectrum.setPaused(paused);});
const reset=document.getElementById('reset-view');reset.disabled=false;reset.addEventListener('click',()=>$('#plot-placeholder').trigger('dblclick'));
document.getElementById('spectrum-view').disabled=false;
const persistence=document.getElementById('persistence-enabled'); persistence.checked=true;persistence.dispatchEvent(new Event('change'));
window.addEventListener('pagehide',()=>{spectrum.dispose();panel.dispose();basics.plot?.shutdown();});
</script></body></html>`;
    dom.window.close(); return html;
}
if (require.main===module) {
    const output=path.resolve(root,process.argv.find(arg=>arg.startsWith('--output='))?.slice(9)||'tmp/plotting/persistence-preview.html');
    fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,previewHTML());console.log(output);
}
module.exports={previewHTML};
