"""SPI state and transfer contracts, with kernel calls captured by a fixture."""
import unittest
from cpp_fixture import compile_cpp, run_cpp


class SpiTransfersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/spi_transfers.cpp', 'server/hardware/spi_manager.cpp',
            flags=('-fno-exceptions', '-DNDEBUG', '-Wl,--wrap=open', '-Wl,--wrap=open64',
                   '-Wl,--wrap=ioctl', '-Wl,--wrap=__ioctl_time64', '-Wl,--wrap=close',
                   '-Wl,--wrap=read', '-Wl,--wrap=write'))

    def test_cases(self):
        for case in (
            'checked-byte-bounds', 'checked-byte-counts', 'checked-byte-errors',
            'checked-init', 'checked-invalid-device', 'checked-settings', 'checked-transfer',
            'checked-byte-short',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case)


if __name__ == '__main__':
    unittest.main()
