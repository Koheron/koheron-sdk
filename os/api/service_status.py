"""Read live systemd state through the libsystemd already used by the image."""
import ctypes
import errno
import os
import time


_sd = ctypes.CDLL('libsystemd.so.0')
_libc = ctypes.CDLL(None)
_pointer = ctypes.c_void_p
_string = ctypes.c_char_p
_output = ctypes.POINTER(_pointer)

# Declare pointer widths explicitly: the rootfs supports both ARM32 and ARM64.
for _name, _arguments, _result in (
    ('sd_bus_open_system', [_output], ctypes.c_int),
    ('sd_bus_is_ready', [_pointer], ctypes.c_int),
    ('sd_bus_process', [_pointer, _pointer], ctypes.c_int),
    ('sd_bus_wait', [_pointer, ctypes.c_uint64], ctypes.c_int),
    ('sd_bus_set_method_call_timeout', [_pointer, ctypes.c_uint64], ctypes.c_int),
    ('sd_bus_path_encode', [_string, _string, _output], ctypes.c_int),
    ('sd_bus_get_property_string',
     [_pointer, _string, _string, _string, _string, _pointer, _output], ctypes.c_int),
    ('sd_bus_close_unref', [_pointer], _pointer),
):
    _function = getattr(_sd, _name)
    _function.argtypes = _arguments
    _function.restype = _result
_libc.free.argtypes = [_pointer]
_libc.free.restype = None


def _check(result):
    if result < 0:
        raise OSError(-result, os.strerror(-result))
    return result


def _remaining_us(deadline):
    remaining = deadline - time.monotonic_ns()
    if remaining <= 0:
        raise OSError(errno.ETIMEDOUT, os.strerror(errno.ETIMEDOUT))
    return max(1, remaining // 1000)


def unit_is_active(unit):
    """Match systemctl is-active without spawning a process or caching state.

    Each call owns its bus connection, including calls before uWSGI forks.
    Errors propagate as OSError so the API can report unknown live status.
    """
    bus = _pointer()
    path = _pointer()
    state = _pointer()
    deadline = time.monotonic_ns() + 5_000_000_000
    try:
        _check(_sd.sd_bus_open_system(ctypes.byref(bus)))
        # Method timeouts do not cover bus authentication. Drive connection
        # setup ourselves, with the same deadline used for the property query.
        while _check(_sd.sd_bus_is_ready(bus)) == 0:
            _remaining_us(deadline)
            if _check(_sd.sd_bus_process(bus, None)) > 0:
                continue
            _check(_sd.sd_bus_wait(bus, _remaining_us(deadline)))
        _check(_sd.sd_bus_path_encode(b'/org/freedesktop/systemd1/unit',
                                     unit.encode('utf-8'), ctypes.byref(path)))
        _check(_sd.sd_bus_set_method_call_timeout(bus, _remaining_us(deadline)))
        _check(_sd.sd_bus_get_property_string(
            bus, b'org.freedesktop.systemd1', ctypes.cast(path, _string),
            b'org.freedesktop.systemd1.Unit', b'ActiveState', None,
            ctypes.byref(state)))
        # systemctl also regards reload/refresh as active, including notify-reload.
        return ctypes.string_at(state) in (b'active', b'reloading', b'refreshing')
    finally:
        # sd_bus_path_encode/get_property_string allocate strings for the caller.
        _libc.free(state)
        _libc.free(path)
        _sd.sd_bus_close_unref(bus)
