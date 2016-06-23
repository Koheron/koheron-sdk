import context
import os
import numpy as np

from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient
from project_config import ProjectConfig

from drivers.oscillo import Oscillo

host = os.getenv('HOST','192.168.1.2')
project = os.getenv('NAME','')

im = InstrumentManager(host)
im.install_instrument(project)
pc = ProjectConfig(project)

client = KClient(host)
oscillo = Oscillo(client)
oscillo.reset()

class TestsOscillo:
    def test_get_adc(self):
        adc = oscillo.get_adc()
        assert(np.shape(adc) == (2, 8192))
        assert np.amax(adc[0,:]) <= 8192
        assert np.amin(adc[0,:]) >= -8192

# tests = TestsOscillo()
# tests.test_get_adc()