"""Typed I2C buffers and single-transaction error handling using wrapped I/O."""
import unittest
from cpp_fixture import compile_cpp, run_cpp


class I2cTransfersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = compile_cpp(cls, 'server/tests/i2c_transfers.cpp', 'server/hardware/i2c_manager.cpp',
            flags=('-fno-exceptions', '-pthread',
                   *('-Wl,--wrap=' + symbol for symbol in ('open', 'open64', 'opendir',
                     'readdir', 'readdir64', 'closedir', 'ioctl', '__ioctl_time64', 'read', 'write'))))

    def test_cases(self):
        for case in (
            'address', 'concurrent', 'initialization', 'empty', 'bounds', 'short_read',
            'short_write', 'typed',
        ):
            with self.subTest(case=case):
                run_cpp(self, self.binary, case)


if __name__ == '__main__':
    unittest.main()
