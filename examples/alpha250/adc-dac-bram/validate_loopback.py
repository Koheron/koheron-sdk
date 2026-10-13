#!/usr/bin/env python3
"""Validate both ALPHA250 RF loopbacks, including first captures after edits.

Set HOST and connect DAC0 → ADC0 and DAC1 → ADC1. Checks 200 and 250 MS/s.
Saves measured residuals and representative raw captures, without a settling
sleep or a discarded capture after waveform changes.
Requires correct array argument decoding, completed BRAM acquisition readout,
and validated converter timing. This analog loopback check includes those
paths; a failure does not by itself identify a network error.
"""
import json
import os
from pathlib import Path

import numpy as np
from koheron import command, connect
from adc_dac_bram import AdcDacBram

HOST = os.getenv('HOST', '192.168.1.105')
INSTRUMENT = os.getenv('INSTRUMENT_NAME', 'adc-dac-bram')
OUTPUT = Path(os.getenv('LOOPBACK_OUTPUT', 'loopback-validation'))
FREQUENCIES_MHZ = (1, 5, 10, 12, 15, 20, 30, 39, 40, 41, 50, 80)
REPEATS = int(os.getenv('LOOPBACK_REPEATS', '20'))
MAX_RESIDUAL_CODES = 200


class Clock:
    def __init__(self, client):
        self.client = client

    @command(classname='ClockGenerator')
    def get_adc_sampling_freq(self):
        return self.client.recv_double()


def main():
    if REPEATS < 1:
        raise ValueError('LOOPBACK_REPEATS must be positive')
    OUTPUT.mkdir(exist_ok=True)
    client = connect(HOST, name=INSTRUMENT, restart=False)
    clock = Clock(client)
    original_fs = clock.get_adc_sampling_freq()
    if original_fs not in (200e6, 250e6):
        raise RuntimeError('Validation requires a 200 or 250 MS/s initial clock')
    driver = AdcDacBram(client)
    results = []
    try:
        for index, fs in ((1, 250e6), (0, 200e6)):
            driver.set_sampling_frequency(index)
            if clock.get_adc_sampling_freq() != fs:
                raise RuntimeError(f'Could not select {fs:g} samples/s')
            times = np.arange(driver.adc_size) / fs
            for target in FREQUENCIES_MHZ:
                bases = []
                frequencies = []
                for channel in range(2):
                    cycles = round(target * 1e6 * driver.dac_size / fs) + 7*channel
                    frequency = cycles * fs / driver.dac_size
                    frequencies.append(frequency)
                    driver.dac[channel] = .45 * np.sin(2*np.pi*cycles*np.arange(driver.dac_size)/driver.dac_size)
                    basis = np.column_stack((np.sin(2*np.pi*frequency*times),
                                             np.cos(2*np.pi*frequency*times), np.ones(times.size)))
                    bases.append((basis, np.linalg.pinv(basis)))
                driver.set_dac()
                # The first returned buffer must already be complete and fresh.
                for repeat in range(REPEATS):
                    driver.get_adc()
                    for channel, (basis, inverse) in enumerate(bases):
                        adc = driver.adc[channel]
                        fit = basis @ (inverse @ adc)
                        residual = adc - fit
                        outliers = int(np.count_nonzero(abs(residual) > MAX_RESIDUAL_CODES))
                        if np.hypot(*(inverse @ adc)[:2]) < 1000 or np.max(abs(adc)) >= 32760:
                            raise RuntimeError(f'Missing/clipped loopback on ADC{channel}')
                        results.append(dict(sample_rate_hz=fs, channel=channel,
                            frequency_hz=frequencies[channel], repeat=repeat, samples=int(adc.size),
                            residual_dbc=float(10*np.log10(np.var(residual)/np.var(fit))),
                            max_residual_codes=float(abs(residual).max()), outliers=outliers))
                        if repeat == 0 or outliers:
                            np.save(OUTPUT / f'{int(fs/1e6)}MSps-{target}MHz-adc{channel}-{repeat}.npy', adc)
                print(f'{fs/1e6:g} MS/s, {target} MHz: '
                      f'{sum(r["outliers"] for r in results[-2*REPEATS:])} outliers', flush=True)
    finally:
        driver.dac[:]=0
        driver.set_dac()
        driver.set_sampling_frequency(0 if original_fs == 200e6 else 1)
        client.sock.close()
        (OUTPUT / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    total = sum(r['outliers'] for r in results)
    print(f'{sum(r["samples"] for r in results):,} samples; {total} residual outliers')
    if total:
        raise RuntimeError(f'Residual exceeded {MAX_RESIDUAL_CODES} codes; inspect saved captures')


if __name__ == '__main__':
    main()
