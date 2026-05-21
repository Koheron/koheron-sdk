import os
import time
import numpy as np
from koheron import connect

from phase_noise_analyzer import PhaseNoiseAnalyzer
from phase_noise_plot import (
    compute_crossovers,
    plot_publication_trace,
    save_segments,
)


def make_segment_configs(
    f_start=2.0,
    f_stop=200.0,
    ratio=np.sqrt(10.0),
    base_count=40,
    alpha=0.5,
    max_count=300,
):
    configs = []
    f = f_start

    while f <= f_stop * 1.001:
        min_count = base_count * (f / f_start) ** alpha
        min_count = min(int(round(min_count)), max_count)

        configs.append({
            "min_frequency": float(f),
            "min_count": min_count,
        })

        f *= ratio

    return configs


def acquire_segment(driver, min_frequency, min_count):
    driver.set_min_frequency(min_frequency)
    time.sleep(1.0)

    freqs, f_min, f_max, phase_noise, smoothed = driver.phase_noise(
        min_count=min_count,
        verbose=True,
    )

    return {
        "min_frequency": min_frequency,
        "min_count": min_count,
        "freqs": freqs,
        "f_min": f_min,
        "f_max": f_max,
        "phase_noise": phase_noise,
        "smoothed": smoothed,
    }


def main():
    host = os.getenv("HOST", "192.168.1.111")
    driver = PhaseNoiseAnalyzer(connect(host, "phase-noise-analyzer"))

    f_dut = 10e6
    # f_dut = 80e6
    f_ref = 10.000000637e6

    driver.set_reference_clock(0)

    driver.set_local_oscillator(0, f_dut)
    driver.set_local_oscillator(1, f_ref)
    driver.set_local_oscillator(2, f_dut)
    driver.set_local_oscillator(3, f_ref)

    xcorr_factor = 100

    segment_configs = make_segment_configs(
        f_start=1.0,
        f_stop=1500.0,
        ratio=np.sqrt(10.0),
        base_count=40 * xcorr_factor,
        alpha=0.5,
        max_count=300 * xcorr_factor,
    )

    # segment_configs = make_segment_configs(
    #     f_start=3.0,
    #     f_stop=1500.0,
    #     ratio=np.sqrt(10.0),
    #     base_count=40 * xcorr_factor,
    #     alpha=0.5,
    #     max_count=300 * xcorr_factor,
    # )

    print(segment_configs)

    segments = [
        acquire_segment(
            driver,
            cfg["min_frequency"],
            cfg["min_count"],
        )
        for cfg in segment_configs
    ]

    segments = sorted(segments, key=lambda seg: seg["f_min"])

    metadata = {
        "host": host,
        "f_dut": f_dut,
        "f_ref": f_ref,
        "segment_configs": segment_configs,
        "created_unix": time.time(),
    }

    filename = "phase_noise_segments.npy"
    save_segments(filename, segments, metadata=metadata)

    crossovers = compute_crossovers(segments)
    plot_publication_trace(segments, crossovers)


if __name__ == "__main__":
    main()