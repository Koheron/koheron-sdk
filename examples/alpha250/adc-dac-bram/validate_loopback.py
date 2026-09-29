#!/usr/bin/env python3
"""Validate DAC0 → ADC0 at the build sampling rate using the loaded adc-dac-bram instrument.

Set HOST if needed. Run after loading/restarting the instrument, at normal
startup phase. Saves measured residuals and representative raw captures.
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
    if fs not in (100e6, 200e6, 240e6, 250e6):
        raise RuntimeError(f'Unsupported sampling rate: {fs}')
    driver = AdcDacBram(client)
    times = np.arange(driver.adc_size) / fs
    results = []
    for target in FREQUENCIES_MHZ:
        if target * 1e6 >= fs / 2:
            continue
        cycles = round(target * 1e6 * driver.dac_size / fs)
        frequency = cycles * fs / driver.dac_size
        driver.dac[0] = .8 * np.sin(2 * np.pi * cycles * np.arange(driver.dac_size) / driver.dac_size)
        driver.set_dac()
        # Prime the BRAM with this periodic tone. get_adc() currently starts a
        # new acquisition before reading, so it can return the previous buffer.
        # The reply is a barrier: the trigger has reached the server before sleep.
        driver.trigger_acquisition()
        driver.get_adc_size()
        time.sleep(.01)
        basis = np.column_stack((np.sin(2*np.pi*frequency*times),
                                 np.cos(2*np.pi*frequency*times), np.ones(times.size)))
        inverse = np.linalg.pinv(basis)
        for repeat in range(REPEATS):
            driver.get_adc()
            adc = driver.adc[0]
            coefficients = inverse @ adc
            amplitude = float(np.hypot(*coefficients[:2]))
            fit = basis @ coefficients
            residual = adc - fit
            outliers = int(np.count_nonzero(abs(residual) > MAX_RESIDUAL_CODES))
            results.append(dict(frequency_hz=frequency, repeat=repeat, samples=int(adc.size),
                amplitude_codes=amplitude, residual_dbc=float(10*np.log10(np.var(residual)/np.var(fit))),
                max_residual_codes=float(abs(residual).max()), outliers=outliers))
            if repeat == 0 or outliers or amplitude < 1000:
                np.save(OUTPUT / f'{target}MHz-{repeat}.npy', adc)
        print(f'{target} MHz: {sum(r["outliers"] for r in results[-REPEATS:])} outliers', flush=True)
    (OUTPUT / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    total = sum(r['outliers'] for r in results)
    if any(r['amplitude_codes'] < 1000 for r in results):
        raise RuntimeError('Missing/weak carrier: check DAC0 to ADC0 loopback and saved captures')
    print(f'{sum(r["samples"] for r in results):,} samples; {total} residual outliers')
    if total:
        raise RuntimeError(f'Residual exceeded {MAX_RESIDUAL_CODES} codes; inspect saved captures')


if __name__ == '__main__':
    main()
