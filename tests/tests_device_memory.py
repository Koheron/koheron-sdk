import context
import os

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
    def test_write32(self):
        value = 42
        dvm.write32('config', pc.cfg['led'], value)
        assert(dvm.read32('config', pc.cfg['led']) == value)

# tests = TestsDeviceMemory()
# tests.test_write32()