import visa
import os
import numpy as np
import time
import matplotlib.pyplot as plt
from scipy import stats

from fft import FFT, Window
from koheron import connect

# Both the Koheron Alpha and the Keysight 33600A must be
# disciplined on the Tektronix MCA3027 OCXO 10 MHz output.

class Keysight33600A:
    def __init__(self):
        import visa
        rm = visa.ResourceManager('@py')
        self.inst = rm.open_resource('TCPIP::A-33600-00000.local::inst0::INSTR')
        self.inst.write('OUTP1:LOAD INF') # High impedance output
        self.inst.write('OUTP2:LOAD INF') # High impedance output
        self.channel = 1

    def set_channel(self, channel):
        self.channel = channel

    def set_sinus(self, freq, v_pp, offset):
        self.inst.write('SOUR' + str(int(self.channel)) + ':APPLy:SIN ' + str(freq / 1E3) + ' KHZ, '
        	            + str(v_pp) + ' VPP, '
        	            + str(offset) + ' V\n')

    def set_dc_offset(self, offset):
        self.inst.write('SOUR' + str(int(self.channel)) + ':APPLy:DC DEF, DEF, ' + str(offset) + ' V\n')

def calibrate_transfer_function(gene, driver):
    n_pts = driver.n_pts
    fs = driver.get_fs()
    print(n_pts, fs)

    freqs = np.linspace(1e6, 62e6, num=200)
    freqs = np.round(freqs / fs * n_pts) * fs / n_pts

    peak_power = 0.0 * freqs

    for i, freq in enumerate(freqs):
        n = np.uint32(freq / fs * n_pts)
        gene.set_sinus(freq, 0.5, 0)

        time.sleep(0.5)
        psd = driver.read_psd_raw()
        peak_power[i] = psd[n-1]
        #peak_power[i] = np.max(psd)
        print(i, freq, peak_power[i])

    H = peak_power / peak_power[0] # Power transfer function

    # We fit 1 / H(f) with a degree 5 polynomial
    p = np.polyfit(freqs, 1 / H, 5)

    H_db = 10 * np.log10(H)
    H_fit_db = 10 * np.log10(1 / np.polyval(p, freqs))
    residuals_db = H_db - H_fit_db

    f, axarr = plt.subplots(2, sharex=True)
    axarr[1].set_xlabel('Frequency (MHz)')
    axarr[0].set_ylabel('Transfer function (dB)')
    axarr[1].set_ylabel('Residuals (dB)')
    axarr[0].semilogx(freqs * 1e-6, H_db)
    axarr[0].semilogx(freqs * 1e-6, H_fit_db)
    axarr[1].semilogx(freqs * 1e-6, residuals_db)
    plt.show()

    return p

def calibrate_offset_gain(gene, driver, channel):
    n_cal_pts = 100
    voltages = np.linspace(-1, 1, n_cal_pts)
    off_raw = 0 * voltages

    for i, Voff in enumerate(voltages):
        gene.set_dc_offset(Voff)
        time.sleep(0.5)
        off_raw[i] = driver.get_adc_raw_data(100)[channel]
        print(i, off_raw[i])

    gain_lsb, offset_lsb, r_value, p_value, std_err = stats.linregress(voltages, off_raw)
    print(gain_lsb, offset_lsb)

    gain_volts, offset_volts, r_value, p_value, std_err = stats.linregress(off_raw, voltages)
    print(gain_volts, offset_volts)

    residuals = off_raw - (gain_lsb * voltages + offset_lsb)

    f, axarr = plt.subplots(2, sharex=True)
    axarr[0].plot(voltages, off_raw)
    axarr[0].plot(voltages, gain_lsb * voltages + offset_lsb)
    axarr[1].plot(voltages, residuals)
    axarr[0].set_ylabel("ADC raw value")
    axarr[1].set_ylabel("ADC INL")
    axarr[1].set_xlabel("Input voltage (V)")
    plt.show()

    return gain_lsb, offset_lsb

if __name__=="__main__":
    gene = Keysight33600A()
    host = os.getenv('HOST', '192.168.1.18')
    client = connect(host, 'fft', restart=False)
    driver = FFT(client)

    driver.set_fft_window(Window.FLAT_TOP)

    for channel_under_test in range(2):
        gene.set_channel(channel_under_test + 1)
        driver.set_in_channel(channel_under_test)
        time.sleep(1)

        gain, offset = calibrate_offset_gain(gene, driver, channel_under_test)
        p = calibrate_transfer_function(gene, driver)

        cal_coeffs = np.float32(np.concatenate((np.array([gain, offset]), p)))
        print(cal_coeffs)