#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Live Bode analyzer using Koheron ADC/DAC DMA.

This script estimates the frequency response H(f) of a system by exciting the DAC
with white noise and measuring the ADC response via DMA. It computes H(f) from
averaged spectra (Syx / Sxx) and displays a live Bode plot (magnitude + phase).

Features:
- Live plotting during averaging
- Automatic bulk delay estimation/removal
- Baseline (loopback) recording
- DUT measurement with baseline correction

Typical workflow:
1) Record baseline with DAC directly connected to ADC:
     python3 bode_live.py --mode baseline --baseline-path baseline.npz

2) Measure DUT (baseline automatically applied if present):
     python3 bode_live.py --mode dut --baseline-path baseline.npz

"""

import os
import time
import argparse
from koheron import command, connect
import matplotlib.pyplot as plt
import numpy as np

# DMA returns N*N_PTS uint32 words, each packing 2 int16 samples
N_PTS = 64 * 1024
N_DESC_MAX = 256


class AdcDacDma(object):
    def __init__(self, client):
        self.client = client
        self.n = 0
        self.dac = np.zeros((0,), dtype=np.float64)
        self.adc = np.zeros((0,), dtype=np.float64)
        self._alloc_N = None

    @command()
    def select_adc_channel(self, channel):
        pass

    @command()
    def set_dac_data(self, data):
        pass

    def set_dac(self, warning=False):
        if warning and self.dac.size:
            if np.max(np.abs(self.dac)) >= 1.0:
                print("WARNING: dac out of bounds; clipping")

        s = np.clip(self.dac, -1.0, 0.999969482421875)
        i16 = np.round(s * 32767.0).astype(np.int16)
        u16 = i16.view(np.uint16).astype(np.uint32)
        # expects even number of samples
        packed = u16[::2] | (u16[1::2] << 16)
        self.set_dac_data(packed)

    @command()
    def start_dma(self, N):
        pass

    @command()
    def stop_dma(self):
        pass

    @command()
    def get_adc_data_n(self, N):
        return self.client.recv_vector(dtype="uint32", check_type=False)

    @command()
    def bode_reset(self, n_fft, fs):
        pass

    @command()
    def bode_set_baseline(self, h_real, h_imag, mask):
        pass

    @command()
    def bode_clear_baseline(self):
        pass

    @command()
    def bode_acquire_step(self, N, thr_rel, remove_delay, band_lo, band_hi, apply_baseline):
        pass

    @command()
    def bode_get_h_real(self):
        return self.client.recv_vector(dtype="float64")

    @command()
    def bode_get_h_imag(self):
        return self.client.recv_vector(dtype="float64")

    @command()
    def bode_get_mask(self):
        return self.client.recv_vector(dtype="uint8")

    @command()
    def bode_get_tau(self):
        return self.client.recv_double()

    @command()
    def bode_get_count(self):
        return self.client.recv_uint32()

    def ensure_alloc(self, N):
        """Allocate buffers only if N changes."""
        N = int(N)
        if N < 1:
            N = 1
        if N > N_DESC_MAX:
            N = N_DESC_MAX

        if self._alloc_N == N:
            return N

        self._alloc_N = N
        self.n = 2 * N_PTS * N  # int16 samples after unpack
        self.dac = np.zeros((self.n,), dtype=np.float64)
        self.adc = np.zeros((self.n,), dtype=np.float64)
        return N

    def capture_adc(self, N):
        """Capture only; does NOT resize or touch dac."""
        N = int(N)
        self.start_dma(N)
        data = self.get_adc_data_n(N)  # length = N*N_PTS uint32
        self.stop_dma()

        lo = (data & 0xFFFF).astype(np.uint16)
        hi = (data >> 16).astype(np.uint16)

        self.adc[::2] = lo.view(np.int16).astype(np.float64)
        self.adc[1::2] = hi.view(np.int16).astype(np.float64)

def save_baseline(path, f, H, mask, meta=None):
    np.savez(
        path,
        f=f.astype(np.float64),
        H_real=H.real.astype(np.float64),
        H_imag=H.imag.astype(np.float64),
        mask=mask.astype(np.bool_),
        meta=np.array([meta], dtype=object),
    )
    print(f"Saved baseline: {path}")


def load_baseline(path):
    d = np.load(path, allow_pickle=True)
    f = d["f"]
    H = d["H_real"] + 1j * d["H_imag"]
    mask = d["mask"]
    meta = d["meta"][0] if "meta" in d else None
    return f, H, mask, meta


def make_live_plot(f, k0=1, title="Bode (live)"):
    f1 = f[k0:]

    plt.ion()
    fig, (ax_mag, ax_phase) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)
    fig.suptitle(title)

    mag_line, = ax_mag.semilogx(f1, np.zeros_like(f1))
    ax_mag.set_ylabel("Magnitude (dB)")
    ax_mag.grid(True, which="both", linestyle="--", alpha=0.4)

    phase_line, = ax_phase.semilogx(f1, np.zeros_like(f1))
    ax_phase.set_xlabel("Frequency (Hz)")
    ax_phase.set_ylabel("Phase (rad)")
    ax_phase.grid(True, which="both", linestyle="--", alpha=0.4)

    return fig, ax_mag, ax_phase, mag_line, phase_line


def live_bode(
    driver,
    N,
    fs,
    n_fft,
    n_acq,
    amp,
    k0,
    band_lo,
    band_hi,
    thr_rel,
    remove_delay,
    baseline=None,
    update_every=5,
    seed=None,
    title="Bode (live)",
):
    """
    baseline: None or tuple (f0, H0, mask0)
    """
    f = np.fft.rfftfreq(n_fft, d=1.0 / fs)

    fig, ax_mag, ax_phase, mag_line, phase_line = make_live_plot(f, k0=k0, title=title)

    rng = np.random.default_rng(seed)
    driver.bode_reset(n_fft, fs)
    if baseline is not None:
        f0, H0, mask0 = baseline
        if f0.shape != f.shape or np.max(np.abs(f0 - f)) != 0.0:
            raise RuntimeError(
                "Baseline frequency grid doesn't match. Ensure fs/N/n_fft are identical."
            )
        driver.bode_set_baseline(H0.real.astype(np.float64), H0.imag.astype(np.float64), mask0.astype(np.uint8))
    else:
        driver.bode_clear_baseline()

    # for stable y-lims after first draw
    did_autoscale = False
    t0 = time.time()

    for k in range(n_acq):
        # DAC white noise
        white = rng.normal(size=driver.n)
        white /= (np.max(np.abs(white)) + 1e-12)
        driver.dac = amp * white
        driver.set_dac(warning=(k == 0))

        driver.bode_acquire_step(N, thr_rel, remove_delay, band_lo, band_hi, baseline is not None)
        H_used = driver.bode_get_h_real() + 1j * driver.bode_get_h_imag()
        tau = driver.bode_get_tau()

        # update plot occasionally
        if (k % update_every) == 0 or (k == n_acq - 1):
            mag_db = 20.0 * np.log10(np.abs(H_used[k0:]) + 1e-12)
            phase = np.unwrap(np.angle(H_used[k0:]))

            mag_line.set_ydata(mag_db)
            phase_line.set_ydata(phase)

            if not did_autoscale:
                ax_mag.relim()
                ax_mag.autoscale_view()
                ax_phase.relim()
                ax_phase.autoscale_view()
                did_autoscale = True

            fig.canvas.draw()
            fig.canvas.flush_events()

            # lightweight status
            dt = time.time() - t0
            print(f"k={k+1:4d}/{n_acq}  tau={tau*1e9:7.1f} ns  elapsed={dt:6.1f}s")

    plt.ioff()
    plt.show()

    # return final (useful for baseline save)
    H_final = driver.bode_get_h_real() + 1j * driver.bode_get_h_imag()
    mask_final = driver.bode_get_mask().astype(bool)
    return f, H_final, mask_final


def main():
    ap = argparse.ArgumentParser(
        description="Koheron ADC/DAC DMA live Bode estimate with optional baseline correction"
    )
    ap.add_argument("--host", default=os.getenv("HOST", "192.168.1.98"))
    ap.add_argument("--name", default="adc-dac-dma")
    ap.add_argument("--adc-channel", type=int, default=0)
    ap.add_argument("--fs", type=float, default=250e6)
    ap.add_argument("--N", type=int, default=1)
    ap.add_argument("--n-acq", type=int, default=200)
    ap.add_argument("--amp", type=float, default=0.9)
    ap.add_argument("--band-lo", type=float, default=1e6)
    ap.add_argument("--band-hi", type=float, default=5e7)
    ap.add_argument("--thr-rel", type=float, default=1e-6)
    ap.add_argument("--no-delay-remove", action="store_true")
    ap.add_argument("--mode", choices=["baseline", "dut"], default="dut")
    ap.add_argument("--baseline-path", default="baseline.npz", help="baseline file path to save/load")
    ap.add_argument("--update-every", type=int, default=5, help="plot refresh period in acquisitions")
    ap.add_argument("--seed", type=int, default=None)

    # FFT length control:
    ap.add_argument(
        "--use-full-capture",
        action="store_true",
        help="use full captured int16 samples for FFT (2*N*N_PTS). Default matches legacy (N*N_PTS).",
    )

    args = ap.parse_args()

    client = connect(args.host, name=args.name, restart=False)
    driver = AdcDacDma(client)
    driver.select_adc_channel(args.adc_channel)
    N = driver.ensure_alloc(args.N)

    remove_delay = not args.no_delay_remove

    # FFT length: default keeps your original behavior (n_fft = N*N_PTS)
    n_fft = (2 * N * N_PTS) if args.use_full_capture else (N * N_PTS)
    if n_fft > driver.n:
        raise RuntimeError(f"n_fft ({n_fft}) > driver.n ({driver.n})")

    baseline = None
    if args.mode == "dut" and args.baseline_path and os.path.exists(args.baseline_path):
        f0, H0, mask0, meta0 = load_baseline(args.baseline_path)
        baseline = (f0, H0, mask0)
        print(f"Loaded baseline: {args.baseline_path}")
        if meta0 is not None:
            print("Baseline meta:", meta0)

    if args.mode == "baseline":
        title = "Baseline (loopback) Bode (live)"
        f, H0, mask0 = live_bode(
            driver=driver,
            N=N,
            fs=args.fs,
            n_fft=n_fft,
            n_acq=args.n_acq,
            amp=args.amp,
            k0=1,
            band_lo=args.band_lo,
            band_hi=args.band_hi,
            thr_rel=args.thr_rel,
            remove_delay=remove_delay,
            baseline=None,
            update_every=max(1, args.update_every),
            seed=args.seed,
            title=title,
        )
        meta = {
            "host": args.host,
            "name": args.name,
            "adc_channel": args.adc_channel,
            "fs": args.fs,
            "N": N,
            "n_acq": args.n_acq,
            "amp": args.amp,
            "band_lo": args.band_lo,
            "band_hi": args.band_hi,
            "thr_rel": args.thr_rel,
            "delay_removed": remove_delay,
            "n_fft": int(n_fft),
            "use_full_capture": bool(args.use_full_capture),
        }
        save_baseline(args.baseline_path, f, H0, mask0, meta=meta)
        return

    # DUT mode (with optional baseline correction)
    title = "DUT Bode (live)"
    if baseline is not None:
        title = "DUT Bode corrected by baseline (live)"

    live_bode(
        driver=driver,
        N=N,
        fs=args.fs,
        n_fft=n_fft,
        n_acq=args.n_acq,
        amp=args.amp,
        k0=1,
        band_lo=args.band_lo,
        band_hi=args.band_hi,
        thr_rel=args.thr_rel,
        remove_delay=remove_delay,
        baseline=baseline,
        update_every=max(1, args.update_every),
        seed=args.seed,
        title=title,
    )


if __name__ == "__main__":
    main()
