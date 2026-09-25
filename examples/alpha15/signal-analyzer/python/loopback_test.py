"""Check the ALPHA15 signal analyzer with RF DAC0 connected to ADC0."""

import argparse
import os
import time

import numpy as np

from koheron import connect
from decimator import Decimator
from fft import FFT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", nargs="?", default=os.getenv("HOST", "192.168.1.105"))
    parser.add_argument("--instrument", default="signal-analyzer")
    args = parser.parse_args()

    client = connect(args.host, args.instrument)
    fft = FFT(client)
    decimator = Decimator(client)
    fft.select_adc_channel(0)

    # The low-frequency PSD averages 16 FIFO segments. Allow a few extra
    # segments after each tone change so the average contains only new data.
    _, fs_lf, _, transfer_duration_lf, _, _, n_pts = decimator.get_control_parameters()
    settle_time = 20 * transfer_duration_lf
    frequencies = np.arange(n_pts // 2 + 1) * fs_lf / n_pts
    band = (frequencies >= 1000) & (frequencies <= 12000)

    fft.set_test_tone(False)
    try:
        decimator.spectral_density1()  # Start the acquisition thread.
        time.sleep(settle_time)
        off = decimator.spectral_density1()

        fft.set_test_tone(True)
        time.sleep(settle_time)
        on = decimator.spectral_density1()
    finally:
        fft.set_test_tone(False)

    time.sleep(settle_time)
    after = decimator.spectral_density1()

    if not all(np.isfinite(psd).all() and (psd >= 0).all() for psd in (off, on, after)):
        raise RuntimeError("PSD contains a non-finite or negative value")

    peak_index = np.flatnonzero(band)[np.argmax(on[band])]
    peak_frequency = frequencies[peak_index]
    ratio_on_off = on[peak_index] / max(off[peak_index], 1e-30)
    ratio_on_after = on[peak_index] / max(after[peak_index], 1e-30)

    print(f"Peak: {peak_frequency:.2f} Hz")
    print(f"Tone on/off power ratio: {ratio_on_off:.1f}")
    print(f"Tone on/after power ratio: {ratio_on_after:.1f}")

    expected_frequency = 240e6 / 32768
    bin_width = fs_lf / n_pts
    if abs(peak_frequency - expected_frequency) > bin_width or min(ratio_on_off, ratio_on_after) < 100:
        raise RuntimeError("DAC0 to ADC0 loopback tone was not detected")


if __name__ == "__main__":
    main()
