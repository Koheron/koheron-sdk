import numpy as np
import time
from scipy.ndimage import median_filter
from koheron import command

def remove_spurs_db(phase_noise, kernel_size=31, threshold_db=8.0):
    """
    Remove narrow upward spurs before smoothing.

    kernel_size:
        Number of points used for local baseline estimate.
        Must be odd.

    threshold_db:
        Points more than threshold_db above local median are removed.
    """
    cleaned = phase_noise.copy()

    valid = np.isfinite(cleaned)
    if not np.any(valid):
        return cleaned

    if kernel_size % 2 == 0:
        kernel_size += 1

    baseline = np.full_like(cleaned, np.nan)
    baseline[valid] = median_filter(cleaned[valid], size=kernel_size)

    spur_mask = valid & ((cleaned - baseline) > threshold_db)

    cleaned[spur_mask] = np.nan

    return cleaned

def smooth_phase_noise_logfreq(freqs, phase_noise, nstart=1, half_width_decades=0.05):
    N = len(freqs)
    smoothed = np.full_like(phase_noise, np.nan, dtype=float)
    scale = 10.0 ** half_width_decades

    j0 = nstart - 1
    j1 = nstart - 2

    sum_linear = 0.0
    cnt = 0

    def add(j):
        nonlocal sum_linear, cnt
        db = phase_noise[j]
        if np.isfinite(db):
            sum_linear += 10.0 ** (db / 10.0)
            cnt += 1

    def remove(j):
        nonlocal sum_linear, cnt
        db = phase_noise[j]
        if np.isfinite(db):
            sum_linear -= 10.0 ** (db / 10.0)
            cnt -= 1

    for i in range(nstart - 1, N):
        fi = freqs[i]

        if not np.isfinite(fi) or fi <= 0:
            smoothed[i] = np.nan
            continue

        f_min_win = fi / scale
        f_max_win = fi * scale

        while j1 + 1 < N and freqs[j1 + 1] <= f_max_win:
            j1 += 1
            add(j1)

        while j0 < N and freqs[j0] < f_min_win:
            remove(j0)
            j0 += 1

        if cnt > 0:
            smoothed[i] = 10.0 * np.log10(sum_linear / cnt)

    return smoothed

class PhaseNoiseAnalyzer(object):
    def __init__(self, client):
        self.client = client
        self.calib_factor = 4.196
        self.npts = 65536

    def set_local_oscillator(self, channel, freq):
        pass

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass

    @command()
    def set_cic_rate(self, rate):
        pass

    @command()
    def set_min_frequency(self, min_frequency_hz):
        pass

    @command()
    def reset_cumulative_averager(self):
        pass

    @command()
    def get_parameters(self):
        return self.client.recv_tuple('IfIfIddddII')

    def averager_xy_count(self):
        tup = self.get_parameters()
        return tup[10]
    
    def get_freqs(self, npts):
        tup = self.get_parameters()
        fft_size = 2 * (tup[0] - 1)
        fs = tup[1]
        df = fs / fft_size
        f_min = 2 * df
        f_max = 0.75 * 0.5 * fs
        freqs = np.arange(npts) * df
        return [freqs, f_min, f_max]

    @command()
    def set_channel(self, channel):
        pass

    @command()
    def get_carrier_power(self, navg):
        return self.client.recv_double()

    @command()
    def get_data(self):
        return self.client.recv_array(self.npts, dtype='int32')

    @command()
    def get_phase_x(self):
        return self.client.recv_array(self.npts, dtype='float32')
    
    @command()
    def get_phase_y(self):
        return self.client.recv_array(self.npts, dtype='float32')
    
    @command()
    def get_phase_xy_sync(self):
        arr = self.client.recv_array(2 * self.npts, dtype='float32')
        return arr[:self.npts], arr[self.npts:-1]

    @command()
    def get_phase_noise(self):
        return self.client.recv_vector(dtype='float32')

    # Phase noise in dBc/Hz
    def phase_noise(self, min_count=10, remove_spurs=True, verbose=True):
        self.set_channel(3)  # Cross-correlation
        self.reset_cumulative_averager()
        time.sleep(2.0)
        self.reset_cumulative_averager()
        time.sleep(2.0)
        self.reset_cumulative_averager()

        while True:
            count = self.averager_xy_count()

            if count >= min_count:
                break

            if verbose:
                print(f"\rAverages: {count} / {min_count}", end="", flush=True)

            time.sleep(0.1)

        if verbose:
            print(f"\r{min_count} / {min_count}")

        psd = self.get_phase_noise()  # rad²/Hz
        freqs, f_min, f_max = self.get_freqs(psd.size)

        phase_noise = np.full(psd.shape, np.nan, dtype=float)
        mask = psd > 0
        phase_noise[mask] = 10.0 * np.log10(0.5 * psd[mask])

        if remove_spurs:
            phase_noise_for_smoothing = remove_spurs_db(
                phase_noise,
                kernel_size=31,
                threshold_db=8.0,
            )
        else:
            phase_noise_for_smoothing = phase_noise

        nstart = np.searchsorted(freqs, f_min)

        smoothed = smooth_phase_noise_logfreq(
            freqs,
            phase_noise_for_smoothing,
            nstart=nstart,
            half_width_decades=0.05
        )

        return freqs, f_min, f_max, phase_noise, smoothed

    def frequency_noise(self):
        f, psd_dB = self.get_phase_noise()
        psd_freq = psd_dB + 3.0 + 20.0 * np.log10(f)
        return f, psd_freq