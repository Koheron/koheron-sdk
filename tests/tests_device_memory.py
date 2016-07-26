import context
import os
import numpy as np

from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient
from project_config import ProjectConfig

from drivers.device_memory import DeviceMemory

host = os.getenv('HOST','192.168.1.2')
project = os.getenv('NAME','')

im = InstrumentManager(host)
im.install_instrument(project)
pc = ProjectConfig(project)

client = KClient(host)
dvm = DeviceMemory(client)

for mmap in pc.mmaps:
    dvm.add_mmap(mmap)

class TestsDeviceMemory:
    def test_add_mmap(self):
        for mmap in pc.mmaps:
            dvm.add_mmap(mmap)
            mmap_params = dvm.get_map_params(mmap.name)
            assert len(mmap_params) == 5
            phys_addr = hex(mmap_params[2])
            assert phys_addr == mmap.offset
            size = mmap_params[3]
            assert size == 1024 * int(mmap.range.replace("K", ""))

    def test_write_read(self):
        value = np.random.randint(16384, size=1)[0]
        dvm.write32('config', pc.cfg['led'], value)
        assert dvm.read32('config', pc.cfg['led']) == value

    def test_write_read_buffer(self):
        buff = np.random.randint(16384, size=2048)
        dvm.write_buffer('dac1', 0, buff)
        buff_ret = dvm.read_buffer('dac1', 0, len(buff))
        assert np.array_equal(buff, buff_ret)

# tests = TestsDeviceMemory()
# tests.test_add_mmap()
# tests.test_write_read()
# tests.test_write_read_buffer()