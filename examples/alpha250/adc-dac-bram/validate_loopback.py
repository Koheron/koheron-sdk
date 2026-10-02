#!/usr/bin/env python3
"""Validate DAC0 → ADC0 at 250 MS/s using the loaded adc-dac-bram instrument.

Set HOST if needed. Run after loading/restarting the instrument, at normal
phase 56. Saves measured residuals and representative raw captures.
Requires correct array argument decoding, completed BRAM acquisition readout,
and validated converter timing. This analog loopback check includes those
paths; a failure does not by itself identify a network error.
"""
import json
import os
from pathlib import Path
import time

import numpy as np
from koheron import command, connect
from adc_dac_bram import AdcDacBram

HOST = os.getenv('HOST', '192.168.1.105')
OUTPUT = Path('loopback-validation')
FREQUENCIES_MHZ = (1, 5, 10, 12, 15, 20, 30, 39, 40, 41, 50)
REPEATS = 20
MAX_RESIDUAL_CODES = 200


class Clock:
    def __init__(self, client):
        self.client = client

    @command(classname='ClockGenerator')
    def get_adc_sampling_freq(self):
        return self.client.recv_double()


def main():
    OUTPUT.mkdir(exist_ok=True)
    client = connect(HOST, name='adc-dac-bram', restart=False)
    fs = Clock(client).get_adc_sampling_freq()
    if fs != 250e6:
        raise RuntimeError('This validation requires the default 250 MS/s configuration')
    driver = AdcDacBram(client)
    times = np.arange(driver.adc_size) / fs
    results = []
    for target in FREQUENCIES_MHZ:
        cycles = round(target * 1e6 * driver.dac_size / fs)
        frequency = cycles * fs / driver.dac_size
        driver.dac[0] = .8 * np.sin(2 * np.pi * cycles * np.arange(driver.dac_size) / driver.dac_size)
        driver.set_dac()
        time.sleep(.01)
        basis = np.column_stack((np.sin(2*np.pi*frequency*times),
                                 np.cos(2*np.pi*frequency*times), np.ones(times.size)))
        inverse = np.linalg.pinv(basis)
        for repeat in range(REPEATS):
            driver.get_adc()
            adc = driver.adc[0]
            fit = basis @ (inverse @ adc)
            residual = adc - fit
            outliers = int(np.count_nonzero(abs(residual) > MAX_RESIDUAL_CODES))
            results.append(dict(frequency_hz=frequency, repeat=repeat, samples=int(adc.size),
                residual_dbc=float(10*np.log10(np.var(residual)/np.var(fit))),
                max_residual_codes=float(abs(residual).max()), outliers=outliers))
            if repeat == 0 or outliers:
                np.save(OUTPUT / f'{target}MHz-{repeat}.npy', adc)
        print(f'{target} MHz: {sum(r["outliers"] for r in results[-REPEATS:])} outliers', flush=True)
    (OUTPUT / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    total = sum(r['outliers'] for r in results)
    print(f'{sum(r["samples"] for r in results):,} samples; {total} residual outliers')
    if total:
        raise RuntimeError(f'Residual exceeded {MAX_RESIDUAL_CODES} codes; inspect saved captures')


if __name__ == '__main__':
    main()
