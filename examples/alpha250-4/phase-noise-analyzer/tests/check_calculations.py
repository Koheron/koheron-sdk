"""Independent SciPy oracle for the production cross-spectrum pipeline.

Run after compiling check_cross_density.cpp; optional live .npz capture contains
phase[frames,2,32000] and the get_parameters tuple. No hardware writes occur.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import numpy as np
from scipy import signal

ROOT=Path.cwd()
OUT=Path('tmp/tests/alpha250-4-phase-noise-analyzer')
EXE=OUT/'check_cross_density'
FIR=signal.firwin(161,.06,window='blackman',scale=True)

def reference(x,y,fs,remove_drift=True):
    def stages(a):
        a=np.asarray(a,dtype=float)
        if remove_drift:a=signal.detrend(a,type='linear')
        intermediate=signal.lfilter(FIR,[1.],a[:30880])[80:30880:10]
        last=signal.lfilter(FIR,[1.],intermediate)[80:3080:10]
        return a[:30000],intermediate[:3000],last[:300]
    spectra=[]
    for level,(a,b) in enumerate(zip(stages(x),stages(y))):
        rate=float(np.float32(fs/(10**level)))
        _,p=signal.csd(a,b,fs=rate,window=signal.windows.hann(len(a),sym=True),
                       nperseg=len(a),noverlap=0,detrend='constant',scaling='density')
        gain=np.ones(len(p));k=np.arange(len(p))
        for stage in range(level):
            omega=2*np.pi*k/(2*(len(p)-1)*10**(level-stage))
            gain*=np.abs(signal.freqz(FIR,worN=omega)[1])**2
        p*=np.where(gain>.8,1/gain,1)
        spectra.append(p)
    result=spectra[0].copy();result[:30]=spectra[2][:30];result[30:300]=spectra[1][30:300]
    return result

def cpp(frames,fs,detrend,label):
    inp=OUT/f'audit-{label}-input.bin';out=OUT/f'audit-{label}-output.bin'
    np.asarray(frames,dtype='<f4').tofile(inp)
    subprocess.run(['docker','run','--rm','-u',f'{os.getuid()}:{os.getgid()}',
        '-v',f'{ROOT}:/review','-w','/review',os.environ.get('PNA_CPP_IMAGE','cross-armhf:24.04'),str(EXE),str(inp),str(out),str(fs),str(int(detrend))],check=True)
    values=np.fromfile(out,dtype='<f4').reshape(len(frames),15001,2)
    return values[:,:,0].astype(float)+1j*values[:,:,1].astype(float)

def compare(frames,fs,label):
    results={}
    for remove in [False,True]:
        actual=cpp(frames,fs,remove,label+str(remove))
        expected=np.array([reference(x,y,fs,remove) for x,y in frames])
        errors=[]
        for lo,hi in [(2,30),(30,300),(300,11251)]:
            errors.append(float(np.linalg.norm(actual[:,lo:hi]-expected[:,lo:hi])/
                                max(np.linalg.norm(expected[:,lo:hi]),1e-30)))
        # Float FFTs, float FIR coefficients and float phase residuals versus
        # independent double-precision SciPy processing.
        assert max(errors)<.003,(label,remove,errors)
        results[str(remove)]={'relative_complex_rms_errors_by_segment':errors}
        results[str(remove)]['negative_fraction']=float(np.mean(actual.real[:,2:11251]<0))
    return results

rng=np.random.default_rng(709);n=32000;fs=1e6;t=np.arange(n)/fs
x=.001*np.sin(2*np.pi*(8*fs/30000)*t)+.002*np.sin(2*np.pi*(100*fs/30000)*t)+.003*np.sin(2*np.pi*(1000*fs/30000)*t)
x=x.astype(np.float32)
quadrature=np.cos(2*np.pi*(1000*fs/30000)*t).astype(np.float32)*.003
frames=np.array([[x,x],[x,-x],[.003*np.sin(2*np.pi*(1000*fs/30000)*t),quadrature],
                 [x+rng.normal(0,.001,n),x+rng.normal(0,.001,n)],
                 [rng.normal(0,.001,n),rng.normal(0,.001,n)],
                 [x+.0001*np.arange(n)+4,x+.0001*np.arange(n)-3]],dtype=np.float32)
summary={'synthetic':compare(frames,fs,'synthetic')}
a=cpp(frames,fs,True,'signs')
assert np.all(a[0].real>=0) and np.all(a[1].real<=0)
assert np.max(np.abs(a[0].real+a[1].real))<1e-12
assert abs(a[2,1000].real)<.002*abs(a[2,1000].imag) and a[2,1000].imag>0
# A known phase modulation has total phase variance amplitude²/2.
# The high-frequency tone bypasses the software decimation filters.
summary['known_modulation_power_ratios']={}
for k,amplitude in [(8,.001),(100,.002),(1000,.003)]:
    power=np.sum(a[0,k-5:k+6].real)*(fs/30000)
    ratio=float(power/(amplitude**2/2))
    assert abs(ratio-1)<.001,(k,ratio)
    summary['known_modulation_power_ratios'][str(k)]=ratio
if len(sys.argv)>1:
    capture=np.load(sys.argv[1]);live=capture['phase'];live_fs=float(np.float32(capture['parameters'][1]))
    summary['captured']=compare(live,live_fs,'live')
    before=cpp(live,live_fs,False,'live-before').real.mean(axis=0)
    after=cpp(live,live_fs,True,'live-after').real.mean(axis=0)
    f=np.arange(len(before))*live_fs/30000
    summary['captured']['same_frames_before_after']={}
    for lo,hi in [(2*f[1],1000),(1000,10000),(10000,100000),(100000,.75*f[-1])]:
        mask=(f>=lo)&(f<hi)
        summary['captured']['same_frames_before_after'][f'{lo:g}-{hi:g}']={
            'negative_fraction_before_after':[float(np.mean(before[mask]<0)),float(np.mean(after[mask]<0))],
            'signed_mean_before_after':[float(np.mean(before[mask])),float(np.mean(after[mask]))],
            'relative_rms_change':float(np.linalg.norm(after[mask]-before[mask])/max(np.linalg.norm(before[mask]),1e-30))}
(OUT/'calculation-audit-summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
