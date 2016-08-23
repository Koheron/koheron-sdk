import context
import os
import numpy as np

from koheron import load_instrument
from drivers.oscillo import Oscillo

host = os.getenv('HOST','192.168.1.100')
project = os.getenv('NAME','')
client = load_instrument(host, project)

oscillo = Oscillo(client)
oscillo.reset()

class TestsOscillo:
    def test_get_adc(self):
        adc = oscillo.get_adc()
        assert np.shape(adc) == (2, 8192)
        assert np.amax(adc[0,:]) <= 8191
        assert np.amin(adc[0,:]) >= -8192
        assert np.amax(adc[1,:]) <= 8191
        assert np.amin(adc[1,:]) >= -8192

    def test_averaging(self):
        oscillo.set_averaging(False)
        assert oscillo.get_num_average(0) == oscillo.get_num_average(1)
        oscillo.set_averaging(True)
        oscillo.set_n_avg_min(1000)
        oscillo.get_adc()
        num_avg_0 = oscillo.get_num_average(0)
        assert num_avg_0 == oscillo.get_num_average(1)
        assert num_avg_0 >= 1000

    def test_set_dac(self):
        data_send = np.arange(oscillo.wfm_size, dtype='uint32')
        oscillo.set_dac(data_send, 1)
        data_read = oscillo.get_dac_buffer(1)
        data_tmp = np.uint32(np.mod(np.floor(8192 * data_send) + 8192, 16384) + 8192)
        data_read_expect = data_tmp[::2] + (data_tmp[1::2] << 16)
        assert np.array_equal(data_read, data_read_expect)

    def test_set_dac_float(self):
        data_send = np.float32(np.sin(np.arange(oscillo.wfm_size)))
        oscillo.set_dac_float(1, data_send)
        data_read = oscillo.get_dac_buffer(1)
        data_tmp = np.uint32(np.mod(np.floor(8192 * data_send) + 8192, 16384) + 8192)
        data_read_expect = data_tmp[::2] + (data_tmp[1::2] << 16)
        assert np.array_equal(data_read, data_read_expect)
