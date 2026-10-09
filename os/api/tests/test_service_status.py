"""Exercise native-call ownership and error handling without a system bus."""
import ctypes
import errno
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import Mock, patch


spec = importlib.util.spec_from_file_location(
    'service_status', Path(__file__).parents[1] / 'service_status.py')
status = importlib.util.module_from_spec(spec)
spec.loader.exec_module(status)


class ServiceStatusTest(unittest.TestCase):
    def setUp(self):
        self.buffers = []
        self.state = b'active'
        self.opened = []
        self.sd = Mock()
        self.libc = Mock()
        self.sd.sd_bus_open_system.side_effect = self.open_bus
        self.sd.sd_bus_is_ready.return_value = 1
        self.sd.sd_bus_process.return_value = 0
        self.sd.sd_bus_wait.return_value = 0
        self.sd.sd_bus_set_method_call_timeout.return_value = 0
        self.sd.sd_bus_path_encode.side_effect = self.encode_path
        self.sd.sd_bus_get_property_string.side_effect = self.property
        for name, value in (('_sd', self.sd), ('_libc', self.libc)):
            replacement = patch.object(status, name, value)
            replacement.start()
            self.addCleanup(replacement.stop)

    def pointer(self, output, address):
        ctypes.cast(output, ctypes.POINTER(ctypes.c_void_p))[0] = address

    def string(self, output, value):
        buffer = ctypes.create_string_buffer(value)
        self.buffers.append(buffer)
        self.pointer(output, ctypes.addressof(buffer))
        return 0

    def open_bus(self, output):
        address = len(self.opened) + 1
        self.opened.append(address)
        self.pointer(output, address)
        return 0

    def encode_path(self, prefix, unit, output):
        self.assertEqual(prefix, b'/org/freedesktop/systemd1/unit')
        self.assertEqual(unit, b'test-service.service')
        return self.string(output, prefix + b'/test_2dservice_2eservice')

    def property(self, bus, destination, path, interface, member, error, output):
        self.assertEqual(destination, b'org.freedesktop.systemd1')
        self.assertEqual(path.value,
                         b'/org/freedesktop/systemd1/unit/test_2dservice_2eservice')
        self.assertEqual(interface, b'org.freedesktop.systemd1.Unit')
        self.assertEqual(member, b'ActiveState')
        self.assertIsNone(error)
        return self.string(output, self.state)

    def test_matches_systemctl_active_states(self):
        for state in (b'active', b'reloading', b'refreshing', b'inactive',
                      b'failed', b'activating', b'deactivating', b'maintenance'):
            with self.subTest(state=state):
                self.state = state
                self.assertEqual(status.unit_is_active('test-service.service'),
                                 state in (b'active', b'reloading', b'refreshing'))

    def test_calls_query_fresh_and_releases_both_allocated_strings(self):
        self.assertTrue(status.unit_is_active('test-service.service'))
        self.state = b'failed'
        self.assertFalse(status.unit_is_active('test-service.service'))
        self.assertEqual(self.sd.sd_bus_open_system.call_count, 2)
        self.assertEqual(self.sd.sd_bus_get_property_string.call_count, 2)
        self.assertEqual([call.args[0].value for call in
                          self.sd.sd_bus_close_unref.call_args_list], [1, 2])
        freed = [call.args[0].value for call in self.libc.free.call_args_list]
        expected = [ctypes.addressof(buffer) for buffer in self.buffers]
        self.assertCountEqual(freed, expected)
        self.assertEqual(len(freed), 4)
        for call in self.sd.sd_bus_set_method_call_timeout.call_args_list:
            self.assertGreater(call.args[1], 0)
            self.assertLessEqual(call.args[1], 5_000_000)

    def test_connection_failure_does_not_query_or_leave_a_bus_open(self):
        self.sd.sd_bus_open_system.side_effect = None
        self.sd.sd_bus_open_system.return_value = -errno.ECONNREFUSED
        with self.assertRaises(OSError) as error:
            status.unit_is_active('test-service.service')
        self.assertEqual(error.exception.errno, errno.ECONNREFUSED)
        self.sd.sd_bus_get_property_string.assert_not_called()
        self.assertIsNone(self.sd.sd_bus_close_unref.call_args.args[0].value)
        self.assertTrue(all(call.args[0].value is None
                            for call in self.libc.free.call_args_list))

    def test_native_failures_release_everything_allocated_before_failure(self):
        for name in ('sd_bus_set_method_call_timeout', 'sd_bus_path_encode',
                     'sd_bus_get_property_string'):
            with self.subTest(operation=name):
                self.libc.free.reset_mock()
                self.sd.sd_bus_close_unref.reset_mock()
                self.buffers = []
                with patch.object(self.sd, name, return_value=-errno.ETIMEDOUT):
                    with self.assertRaises(OSError) as error:
                        status.unit_is_active('test-service.service')
                self.assertEqual(error.exception.errno, errno.ETIMEDOUT)
                freed = [call.args[0].value for call in self.libc.free.call_args_list
                         if call.args[0].value is not None]
                self.assertCountEqual(freed, [ctypes.addressof(buffer)
                                             for buffer in self.buffers])
                self.assertEqual(self.sd.sd_bus_close_unref.call_count, 1)

    def test_authentication_timeout_releases_bus_without_querying_state(self):
        self.sd.sd_bus_is_ready.return_value = 0
        with patch.object(status.time, 'monotonic_ns',
                          side_effect=[0, 1_000_000_000, 2_000_000_000, 5_000_000_001]):
            with self.assertRaises(OSError) as error:
                status.unit_is_active('test-service.service')
        self.assertEqual(error.exception.errno, errno.ETIMEDOUT)
        self.sd.sd_bus_wait.assert_called_once()
        self.assertEqual(self.sd.sd_bus_wait.call_args.args[1], 3_000_000)
        self.sd.sd_bus_get_property_string.assert_not_called()
        self.assertEqual(self.sd.sd_bus_close_unref.call_count, 1)

    def test_authentication_processing_rechecks_readiness_before_waiting(self):
        self.sd.sd_bus_is_ready.side_effect = [0, 1]
        self.sd.sd_bus_process.return_value = 1
        self.assertTrue(status.unit_is_active('test-service.service'))
        self.sd.sd_bus_wait.assert_not_called()


if __name__ == '__main__':
    unittest.main()
