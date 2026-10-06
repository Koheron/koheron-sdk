"""Display the DPLL's shared PNA spectrum without changing loop settings."""
import argparse
import os

import numpy as np
from koheron import command, connect
from koheron.phase_noise import get_phase_snapshot, get_spectrum_snapshot


class Dma:
    """Thin client for the shared PNA acquisition owned by the DPLL monitor."""
    npts = 65536
    get_phase_snapshot = get_phase_snapshot
    get_spectrum_snapshot = get_spectrum_snapshot

    def __init__(self, client):
        self.client = client

    @command()
    def get_data_size(self):
        return self.client.recv_uint32()

    @command()
    def get_data(self):
        # CIC/FIR and precision correction are already applied by the server.
        return self.client.recv_array(self.npts, dtype='float32')

    @command()
    def get_sampling_frequency(self):
        return self.client.recv_uint32()


def main():
    from matplotlib import pyplot as plt

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('hosts', nargs='*', default=[os.getenv('HOST', '192.168.1.100')])
    args = parser.parse_args()
    drivers = [Dma(connect(host, name='dpll')) for host in args.hosts]
    fig, ax = plt.subplots()
    lines = [ax.semilogx([], [], label=host)[0] for host in args.hosts]
    sequences = [None] * len(drivers)
    ax.set(xlabel='Offset frequency (Hz)', ylabel='Phase noise (dBc/Hz)', ylim=(-170, -70))
    ax.grid(True, which='both')
    ax.legend()
    plt.show(block=False)
    while plt.fignum_exists(fig.number):
        for i, driver in enumerate(drivers):
            metadata, density = driver.get_spectrum_snapshot()
            sequence, state, _, fs = metadata[:4]
            if state != 1:
                lines[i].set_data([], [])
                sequences[i] = None
                continue
            if sequence == sequences[i]:
                continue
            sequences[i] = sequence
            frequency = np.arange(density.size) * fs / (2 * (density.size - 1))
            with np.errstate(divide='ignore', invalid='ignore'):
                lines[i].set_data(frequency[1:], 10 * np.log10(density[1:] / 2))
            ax.set_xlim(frequency[1], frequency[-1])
        fig.canvas.draw_idle()
        plt.pause(0.1)


if __name__ == '__main__':
    main()
