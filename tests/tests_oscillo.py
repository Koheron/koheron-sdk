import context
import os
import numpy as np

from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient
# from project_config import ProjectConfig

from drivers.oscillo import Oscillo

host = os.getenv('HOST','192.168.1.2')
project = os.getenv('NAME','oscillo')

im = InstrumentManager(host)
im.install_instrument(project, always_restart=True)
# pc = ProjectConfig(project)

client = KClient(host)
oscillo = Oscillo(client)
oscillo.reset()

class TestsOscillo:
    def test_get_adc(self):
        adc = oscillo.get_adc()
        assert np.shape(adc) == (2, 8192)
        assert np.amax(adc[0,:]) <= 8192
        assert np.amin(adc[0,:]) >= -8192
        assert np.amax(adc[1,:]) <= 8192
        assert np.amin(adc[1,:]) >= -8192

    def test_averaging(self):
        oscillo.set_averaging(False)
        assert oscillo.get_num_average_0() == oscillo.get_num_average_1()
        oscillo.set_averaging(True)
        oscillo.set_n_avg_min(1000)
        oscillo.get_adc()
        num_avg_0 = oscillo.get_num_average_0()
        assert num_avg_0 == oscillo.get_num_average_1()
        assert num_avg_0 >= 1000

tests = TestsOscillo()
tests.test_get_adc()
tests.test_averaging()