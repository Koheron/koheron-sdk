#!/usr/bin/env python3
"""Explore wider phase arithmetic; not an RTL, pipeline or hardware validation.

Uses the standalone test's independently generated atan2 vectors. Candidate
phase words are 24 bits, with eight additional internal angle fraction bits.
Coordinate normalization, shrinking residuals and frozen late x match the
custom extractor's arithmetic approach. Outputs include every input marked
valid outside reset; pipeline flushing is not modeled.
"""
import math
import sys
from pathlib import Path
if len(sys.argv) != 2:
    raise SystemExit("Usage: precision_model.py vectors.txt")
vectors = []
for line in Path(sys.argv[1]).read_text().splitlines():
    x,y,_,valid,reset,phase=line.split()
    if int(valid) and int(reset): vectors.append((int(x),int(y),float(phase)*math.pi/8192))
def signed(value,width):
    return ((value+(1<<(width-1))) & ((1<<width)-1))-(1<<(width-1))
for n in [16,20,22,24]:
    angles=[round(math.atan(2**-k)/math.pi*2**29) for k in range(n)]
    peak=raw_peak=square=raw_square=0.
    for xi,qi,expected in vectors:
        if not (xi or qi): actual=raw=0.
        else:
            shift=23-(abs(xi)|abs(qi)).bit_length()+1
            x,y=(abs(xi)<<shift,(-qi if xi<0 else qi)<<shift)
            z=(-2**29 if qi<0 else 2**29) if xi<0 else 0
            for k,a in enumerate(angles):
                negative=y<0
                nx=x if k>=8 else x-(y>>k) if negative else x+(y>>k)
                ny=y+(x>>k) if negative else y-(x>>k)
                z=z-a if negative else z+a
                x=signed(nx,27)
                y=signed(ny,27-max(k-1,0))
            raw=signed(z,30)*math.pi/2**29
            actual=signed((z+128)>>8,22)*math.pi/2**21
        raw_error=abs((raw-expected+math.pi)%(2*math.pi)-math.pi)
        error=abs((actual-expected+math.pi)%(2*math.pi)-math.pi)
        peak=max(peak,error); raw_peak=max(raw_peak,raw_error)
        square+=error*error;raw_square+=raw_error*raw_error
    print(f'rotations={n} samples={len(vectors)} output24_peak_urad={peak*1e6:.6f} output24_rms_urad={(square/len(vectors))**.5*1e6:.6f} raw_peak_urad={raw_peak*1e6:.6f} raw_rms_urad={(raw_square/len(vectors))**.5*1e6:.6f}')
