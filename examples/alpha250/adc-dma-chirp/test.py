"""One acquisition; plot DAC0 -> cable -> ADC0 magnitude and phase."""
import matplotlib.pyplot as plt
import numpy as np
from koheron import connect
from koheron.alpha250 import Alpha250

from adc_dma import AdcDma
from chirp import Chirp, reference, transfer_response


def main():
    host = "192.168.1.105"
    chirp = Chirp(start_hz=500, stop_hz=110e6, duration=0.250)
    client = connect(host, name="adc-dma-chirp", restart=False)
    Alpha250(client).set_sampling_frequency(1)  # 250 MS/s
    driver = AdcDma(client)
    print("Capturing 64 Mi samples from ADC0...")
    adc = driver.acquire(chirp)
    print("Reconstructing the deterministic reference...")
    excitation = reference(chirp)
    frequency, response = transfer_response(adc, excitation)
    fig, (magnitude, phase) = plt.subplots(2, 1, sharex=True, figsize=(10, 7))
    magnitude.semilogx(frequency, 20 * np.log10(np.maximum(np.abs(response), 1e-15)))
    magnitude.set_ylabel("Magnitude (dB)")
    phase.semilogx(frequency, np.angle(response, deg=True))
    phase.set_ylabel("Phase (degrees, wrapped)")
    phase.set_xlabel("Frequency (Hz)")
    for axis in (magnitude, phase):
        axis.grid(True, which="both")
    fig.suptitle("DAC0 → cable → ADC0")
    fig.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
