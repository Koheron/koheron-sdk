#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
from koheron import command, connect
import matplotlib.pyplot as plt
import numpy as np
import statistics as stats
import math

class AdcDacDma(object):
    def __init__(self, client):
        self.n = 32*1024*1024
        self.client = client
        self.dac = np.zeros((self.n))
        self.adc = np.zeros((self.n))

    @command()
    def select_adc_channel(self, channel):
        pass

    @command()
    def set_dac_data(self, data):
        pass

    def set_dac(self, warning=False, reset=False):
        if warning:
            if np.max(np.abs(self.dac)) >= 1:
                print('WARNING : dac out of bounds')
        dac_data = np.uint32(np.mod(np.floor(32768 * self.dac) + 32768, 65536) + 32768)
        self.set_dac_data(dac_data[::2] + 65536 * dac_data[1::2])

    @command()
    def start_dma(self):
        pass

    @command()
    def stop_dma(self):
        pass

    @command()
    def get_adc_data(self):
        return self.client.recv_array(self.n//2, dtype='uint32')

    @command()
    def get_adc_data_span(self):
        return self.client.recv_array(self.n//2, dtype='uint32', check_type=False)

    def get_adc(self):
        data = self.get_adc_data()
        self.adc[::2] = (np.int32(data % 65536) - 32768) % 65536 - 32768
        self.adc[1::2] = (np.int32(data >> 16) - 32768) % 65536 - 32768

    def get_adc_span(self):
        data = self.get_adc_data_span()
        self.adc[::2] = (np.int32(data % 65536) - 32768) % 65536 - 32768
        self.adc[1::2] = (np.int32(data >> 16) - 32768) % 65536 - 32768

    @command()
    def set_test_vector(self, size):
        pass

    @command()
    def get_test_vector(self):
        return self.client.recv_vector(dtype='uint32', check_type=False)


# --- utilities ---------------------------------------------------------------

def round_bytes_to_u32_elems(n_bytes: int) -> int:
    return max(1, n_bytes // 4)  # uint32 payload; at least 1 element

def sizes_bytes_logspace(min_b=100, max_b=(128*1024*1024), per_decade=6):
    # e.g. 100 B .. 128 MiB, ~6 points per decade
    xs = np.unique(np.logspace(math.log10(min_b), math.log10(max_b),
                               num=int(math.log10(max_b/min_b)*per_decade)+1,
                               base=10).astype(int))
    # ensure we include some “round” sizes
    extras = np.array([
        128, 256, 512,
        1*1024, 2*1024, 4*1024, 8*1024, 16*1024, 32*1024, 64*1024, 128*1024,
        256*1024, 512*1024, 1*1024*1024, 2*1024*1024, 4*1024*1024,
        8*1024*1024, 16*1024*1024, 32*1024*1024, 64*1024*1024, 128*1024*1024
    ], dtype=int)
    xs = np.unique(np.concatenate([xs, extras]))
    return xs

def bench_get_test_vector(driver, sizes_b, rounds_small=20, rounds_large=6, warmup=True):
    """Return dict with arrays: size_bytes, best, median, mean, stdev (MiB/s)."""
    to_mib = 1.0 / (1024.0 * 1024.0)
    print("\nsize(B)\tround/ms\tMiB/s")
    size_bytes = []
    best = []
    median = []
    mean = []
    stdev = []

    for n_b in sizes_b:
        elems = round_bytes_to_u32_elems(n_b)
        n_b = elems * 4  # snap to u32 size

        # choose more rounds for tiny payloads to average out jitter
        rounds = rounds_small if n_b <= 64*1024 else rounds_large

        driver.set_test_vector(elems)
        if warmup:
            _ = driver.get_test_vector()

        samples = []
        for _ in range(rounds):
            t0 = time.perf_counter()
            data = driver.get_test_vector()
            t1 = time.perf_counter()
            if data.size != elems:
                raise RuntimeError(f"Size mismatch: requested {elems}, got {data.size}")
            samples.append(t1 - t0)

        rates = (n_b * to_mib) / np.array(samples)

        size_bytes.append(n_b)
        best.append(np.max(rates))
        median.append(np.median(rates))
        mean.append(np.mean(rates))
        stdev.append(np.std(rates))

        print(f"{n_b}\t   ---  \tbest/med/mean = "
              f"{np.max(rates):.2f}/{np.median(rates):.2f}/{np.mean(rates):.2f} MiB/s\n")

    return {
        "size_bytes": np.array(size_bytes),
        "best": np.array(best),
        "median": np.array(median),
        "mean": np.array(mean),
        "stdev": np.array(stdev),
    }

def plot_rates(results, title="get_test_vector throughput"):
    x = results["size_bytes"]
    plt.figure(figsize=(8,4.8))
    plt.loglog(x, results["best"],   marker='o', linestyle='-', label='best')
    plt.loglog(x, results["median"], marker='s', linestyle='-', label='median')
    plt.loglog(x, results["mean"],   marker='^', linestyle='-', label='mean')
    # error band around mean
    m, s = results["mean"], results["stdev"]
    plt.fill_between(x, m - s, m + s, alpha=0.15, linewidth=0, label='±1σ (mean)')

    plt.xlabel('Payload size (bytes)')
    plt.ylabel('Throughput (MiB/s)')
    plt.title(title)
    plt.grid(True, which='both', linestyle=':', linewidth=0.5)
    plt.legend()
    plt.tight_layout()

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.98')
    client = connect(host, name='tests-network')
    driver = AdcDacDma(client)

    adc_channel = 0
    driver.select_adc_channel(adc_channel)

    # --- Fixed-size ADC recv-only benchmark (64 MiB payload) ---
    print(f"Get ADC{adc_channel} data ({driver.n} points)")
    driver.start_dma()

    for _ in range(5):
        t0 = time.perf_counter()
        data = driver.get_adc_data_span()   # recv-only
        t1 = time.perf_counter()
        took = t1 - t0
        print(f"recv_only: {(64 / took):.1f} MiB/s [took {took:.3f} s]")

    # --- Vector size scan to find copy vs iov threshold ---
    sizes_b = sizes_bytes_logspace(min_b=100, max_b=128*1024*1024, per_decade=8)
    results = bench_get_test_vector(driver, sizes_b, rounds_small=25, rounds_large=8, warmup=True)

    # Print a concise table head/tail
    print("\n=== get_test_vector throughput (head) ===")
    for i in range(min(10, len(sizes_b))):
        n_b = results["size_bytes"][i]
        med = results["median"][i]
        print(f"{n_b:8d} B  ->  {med:6.1f} MiB/s")

    print("\n=== get_test_vector throughput (tail) ===")
    for i in range(max(0, len(sizes_b)-10), len(sizes_b)):
        n_b = results["size_bytes"][i]
        med = results["median"][i]
        print(f"{n_b:8d} B  ->  {med:6.1f} MiB/s")

    # Plot (semilogx)
    plot_rates(results, title="Throughput vs payload size (uint32 vector)")
    plt.show()
