import os
import numpy as np

from koheron import connect
from oscillo import Oscillo

host = os.getenv('HOST','192.168.1.100')
client = connect(host, name='oscillo')

oscillo = Oscillo(client)
oscillo.reset()

class TestsOscillo:
    def test_get_adc(self):
        oscillo.get_adc()
        assert np.shape(oscillo.adc) == (2, oscillo.wfm_size)
        assert np.amax(oscillo.adc[0,:]) <= 8191
        assert np.amin(oscillo.adc[0,:]) >= -8192
        assert np.amax(oscillo.adc[1,:]) <= 8191
        assert np.amin(oscillo.adc[1,:]) >= -8192

    def test_averaging(self):
        oscillo.set_averaging(False)
        assert oscillo.get_num_average(0) == oscillo.get_num_average(1)
        oscillo.set_averaging(True)
        oscillo.set_n_average_min(1000)
        oscillo.get_adc()
        num_avg_0 = oscillo.get_num_average(0)
        assert num_avg_0 == oscillo.get_num_average(1)
        assert num_avg_0 >= 1000

    def test_set_dac(self):
        data_send = np.arange(oscillo.wfm_size, dtype='uint32')
        oscillo.dac[0, :] = data_send
        oscillo.dac[1, :] = data_send
        oscillo.set_dac([0,1])
        data_read = oscillo.get_dac_buffer(1)
        data_tmp = np.uint32(np.mod(np.floor(8192 * data_send) + 8192, 16384) + 8192)
        data_read_expect = data_tmp[::2] + (data_tmp[1::2] << 16)
        assert np.array_equal(data_read, data_read_expect)
        assert np.array_equal(oscillo.get_dac_buffer(0), oscillo.get_dac_buffer(1))

    def test_set_dac_float(self):
        data_send = np.float32(np.sin(np.arange(oscillo.wfm_size)))
        oscillo.set_dac_float(1, data_send)
        data_read = oscillo.get_dac_buffer(1)
        data_tmp = np.uint32(np.mod(np.floor(8192 * data_send) + 8192, 16384) + 8192)
        data_read_expect = data_tmp[::2] + (data_tmp[1::2] << 16)
        assert np.array_equal(data_read, data_read_expect)
