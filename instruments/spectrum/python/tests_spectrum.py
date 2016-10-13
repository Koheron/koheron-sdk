import context
import os
import numpy as np

from koheron import connect
from drivers.spectrum import Spectrum

host = os.getenv('HOST','192.168.1.100')
client = connect(host, name='spectrum')

spectrum = Spectrum(client)
spectrum.reset_acquisition()

class TestsSpectrum:
    def test_set_dac(self):
        data_send = np.arange(spectrum.wfm_size, dtype='uint32')
        spectrum.set_dac(data_send, 1)
        data_read = spectrum.get_dac_buffer(1)
        data_tmp = np.uint32(np.mod(np.floor(8192 * data_send) + 8192, 16384) + 8192)
        data_read_expect = data_tmp[::2] + (data_tmp[1::2] << 16)
        assert np.array_equal(data_read, data_read_expect)
