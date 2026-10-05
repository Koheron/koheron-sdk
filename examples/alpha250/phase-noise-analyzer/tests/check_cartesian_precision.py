"""Weak-PM regression with AMD's installed bit-accurate multiplier/CORDIC.

Compare against atan2 of the full-product filtered integer ADC/LO inputs,
so ADC quantization and image response do not get mistaken for mixer bias.
This is a digital-stage test, not an analog noise-floor calibration.
"""
import json
import os
from pathlib import Path
import subprocess
import zipfile

import numpy as np
from scipy.signal import lfilter

root = Path(__file__).resolve().parents[4]
out = root / "tmp/tests/pna-cartesian-precision"
vendor = out / "vendor"
vendor.mkdir(parents=True, exist_ok=True)
vivado = Path(os.getenv("PNA_VIVADO_PATH", "/tools/Xilinx/2025.1/Vivado"))
for name in ("cmpy", "cordic"):
    archive = vivado / f"data/ip/xilinx/{name}_v6_0/cmodel/{name}_v6_0_bitacc_cmodel_lin64.zip"
    with zipfile.ZipFile(archive) as package:
        package.extractall(vendor)
exe = out / "cartesian_probe"
subprocess.run(["g++", "-O2", "-std=c++17", str(Path(__file__).with_suffix(".cpp")),
                "-I"+str(vendor), "-L"+str(vendor), "-Wl,-rpath-link,"+str(vendor),
                "-lIp_cmpy_v6_0_bitacc_cmodel", "-lIp_cordic_v6_0_bitacc_cmodel",
                "-o", str(exe)], check=True)
env = dict(os.environ, LD_LIBRARY_PATH=str(vendor))

period = 20480
count = 4 * period
n = np.arange(count)
omega = 2 * np.pi * n / period
beta = .001
warmup = 200
basis = np.column_stack((np.sin(omega[warmup:]), np.cos(omega[warmup:]), np.ones(count-warmup)))
taps = np.ones(16)/16
for _ in range(3):
    taps = np.convolve(taps, np.ones(16)/16)


def amplitude(phase):
    phase = np.unwrap(phase)[warmup:]
    coefficients = np.linalg.lstsq(basis, phase, rcond=None)[0]
    return np.hypot(*coefficients[:2])


random = np.random.default_rng(77531)
summary = []
# All PNA sample clocks, two signal levels and thirteen carrier phases.
for fs in (125e6, 200e6, 250e6):
    for peak in (2000, 4800):
        pieces, reference = [], []
        for offset in np.arange(0, 2*np.pi, .5):
            angle = 2 * np.pi * 10e6/fs * n
            # 14-bit ADC aligned into the mixer's signed 16-bit port.
            adc = np.rint(peak * np.cos(angle + offset + beta*np.sin(omega))) * 4
            lo_angle = np.floor((10e6/fs * n % 1)*65536) * 2*np.pi/65536
            lo = np.rint(32766*np.cos(lo_angle)) + 1j*np.rint(32766*np.sin(lo_angle))
            rounding = random.integers(-(1 << 31), 1 << 31, count)
            pieces.append(np.column_stack((adc, lo.real, lo.imag, rounding)).astype("<i4"))
            reference.append(amplitude(np.angle(lfilter(taps, [1], adc*lo))))
        input_path, output_path = out / "input.bin", out / "phase.bin"
        np.concatenate(pieces).tofile(input_path)
        errors = {}
        for width in (16, 24):
            # The multiplier model prints each sample even with debug=0;
            # stage failures still propagate through the probe's exit status.
            subprocess.run([str(exe), str(width), str(input_path), str(output_path)],
                           env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
            measured = np.fromfile(output_path, dtype=np.float64).reshape(len(pieces), count)
            power_error = np.array([(amplitude(p)/ref)**2-1 for p, ref in zip(measured, reference)])
            assert np.isfinite(power_error).all()
            errors[width] = float(np.max(np.abs(power_error)))
        # Widening only phase output leaves the old Cartesian gain error.
        assert errors[16] > .02, errors
        assert errors[24] < .002, errors
        result = dict(sample_rate=fs, adc_peak=peak, max_relative_power_error=errors)
        summary.append(result)
        print(json.dumps(result), flush=True)
(out / "summary.json").write_text(json.dumps(summary, indent=2))
print("Weak PM Cartesian precision checks passed")
