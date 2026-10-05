"""Reproduce LO harmonics with AMD's locally installed bit-accurate model.

No vendor files are redistributed. Run with the SDK Python environment;
PNA_VIVADO_PATH can override /tools/Xilinx/2025.1/Vivado.
"""
import json
import os
from pathlib import Path
import subprocess
import zipfile
import numpy as np

root=Path(__file__).resolve().parents[4]
out=root/'tmp/tests/alpha250-4-phase-noise-analyzer/cordic'
vendor=out/'vendor';vendor.mkdir(parents=True,exist_ok=True)
vivado=Path(os.environ.get('PNA_VIVADO_PATH','/tools/Xilinx/2025.1/Vivado'))
archive=vivado/'data/ip/xilinx/cordic_v6_0/cmodel/cordic_v6_0_bitacc_cmodel_lin64.zip'
with zipfile.ZipFile(archive) as z:z.extractall(vendor)
exe=out/'cordic_probe'
subprocess.run(['g++','-O2','-std=c++17',str(Path(__file__).with_suffix('.cpp')),
                '-I'+str(vendor),'-L'+str(vendor),'-Wl,-rpath-link,'+str(vendor),
                '-lIp_cordic_v6_0_bitacc_cmodel',
                '-o',str(exe)],check=True)
env=dict(os.environ,LD_LIBRARY_PATH=str(vendor))
errors=[];summary={}
for width in [16,24]:
    path=out/f'phase-{width}.bin'
    subprocess.run([str(exe),str(width),'0','131072','3000',str(path)],env=env,check=True)
    error=np.fromfile(path,dtype=np.float64)
    harmonic=np.abs(np.fft.rfft(error)/error.size)
    errors.append(harmonic)
    summary[width]=dict(rms_radians=float(np.std(error)),
                       harmonic_104_radians=float(harmonic[104]),
                       harmonic_204_radians=float(harmonic[204]))
    if width==24:
        # Expectation over all 256 stochastic rounding codes exactly retains
        # the calculated phase; the RTL regression exhausts this property.
        phase_codes=np.rint(error*2**21/np.pi).astype(np.int64)
        for p in phase_codes[::128]:
            assert np.sum((p+np.arange(256))>>8)==p
assert errors[1][104]<errors[0][104]*.01
assert errors[1][204]<errors[0][204]*.01
summary['harmonic_suppression_dB']={str(k):float(20*np.log10(errors[0][k]/errors[1][k])) for k in [104,204]}
(out/'summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
