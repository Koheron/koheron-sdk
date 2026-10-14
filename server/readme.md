* `main/main.cpp`: Executable entry point
* `hardware/`: Hardware services (Memory, FPGA, clocks, busses, ...).
* `runtime/`: OS services (systemd notify, signals, logger).
* `network/`: Network connections + message flow.
* `executor/`: Drivers execution interface.
* `utilities/`: Misc. utilities.
* `drivers/`: Generic drivers for basic IPs (FIFO, DMA, ...).

## Checked SPI operations

`hw::SpiDev` provides `init_checked`, `set_mode_checked`, `set_full_mode_checked`,
`set_speed_checked`, `set_word_length_checked` and `transfer_checked`. They return
`std::expected<void, std::error_code>` and must be checked before using received
data. Kernel errors retain their original `errno` value in the generic category,
even after initialization closes a partially configured descriptor. Initialization
stops at the first failed setting; individual setters update cached settings only
on success.

`transfer_checked` supports the same spans, pointers and arrays as `transfer`.
It reports invalid arguments, oversized requests, invalid descriptors and failed
or short message results. Empty transfers on an open device succeed without an
ioctl. Failed or short messages are not retried, since a retry would start another
SPI transaction.

`write_checked` and `recv_checked` return `std::expected<std::size_t, std::error_code>`.
Their successful value is the actual byte count, including a short transfer or
zero bytes. Compare it with the requested count before using a complete received
payload. Typed buffers retain their native byte representation; counts are bytes,
not elements. Empty buffers on an open device return zero without a system call.
Only `EINTR` is retried. A positive short transfer never triggers another transaction.

The existing integer configuration and transfer methods delegate to the checked
methods, returning `0` or `-1`, logging failures and setting `errno` on failure.
`write` and `recv` retain their byte-count APIs. `recv` returns a short count or zero
at EOF; `write` reports a short or zero write as `-1`/`EIO`. Both stop after a positive
short transfer. Driver RPC signatures are unchanged.
The precision ADC uses checked transfers and starts its acquisition worker only
after SPI configuration and every setup register write succeeds.

## Checked configuration saves

`rt::ConfigManager::save_checked()` returns `std::expected<void, std::error_code>`
from the INI writer. Success follows writing, closing and renaming the temporary
file into place. Directory, write/close and rename failures reach the caller;
the active in-memory settings remain available for retry. `save()` retains its
existing void interface and logs failures. The default file remains
`/etc/koheron/<instrument>/config.ini`; an explicit path can be supplied to the
manager constructor.

Phase-noise analyzers expose `save_config_checked()` as a boolean RPC, while
retaining the existing `save_config()` RPC. The browser waits for the checked
reply and reports a failed save separately from a connection error. Older servers
keep request-only feedback. The single-channel Python client exposes the checked
RPC through `save_config_checked()`.

## Checked memory initialization

`hw::Memory<id>::open_checked()` returns `std::expected<int, MemoryOpenError>`.
The value is the mapped device descriptor. An error contains the map ID and a
`failure` with its backend, operation (`reservation`, `open` or `map`) and error
code. If a `/dev/mem` fallback also fails, `fallback_from` retains the original
custom-device or UIO failure separately from the final error. Errors survive
descriptor cleanup and subsequent retries.

`hw::MemoryManager::open_checked()` attempts every map and returns an expected
void result or a vector of all failed map errors. Successful maps remain open;
a retry preserves their descriptors, pointers and contents. The integer `open()`
methods remain available and log the structured failures before returning `-1`.
They restore `errno` from the final error (`EIO` for a stream error); the manager
uses the first failed map's error.

Fallback policy is unchanged: custom-device and UIO discovery/open failures may
fall back to `/dev/mem`; mapping failures do not trigger another backend. Fixed
DMA windows still require verified exclusion from System RAM before any device
access. `Uio::open_checked()` and `mmap_checked()` also expose expected results,
and UIO filesystem discovery uses error-code overloads with exceptions disabled.

## Checked FPGA loading

`hw::FpgaManager::load_bitstream_checked()` returns
`std::expected<void, FpgaLoadError>`. Each failure identifies the loading stage,
owns the affected path and carries an error code. If removing a failed overlay
also fails, `cleanup` records that error separately, preserving the original
loading failure. `load_bitstream()` retains its integer interface, logs both
errors and restores `errno` from the loading error (`EIO` for a stream error).

Backend discovery occurs on each load, so a missing backend returns an error
instead of exiting from the constructor. `/dev/xdevcfg` retains precedence over
the FPGA manager/configfs backend. The xdevcfg path reads and validates the
bitstream before opening the device, checks the complete write and close, and
does not replay a short write. The overlay path checks programming state and
both the applied overlay path and status. Filesystem operations use error-code
overloads with exceptions disabled; optional device binding and mapping setup
remains best-effort, with failures logged.

The default live, sysfs, device and firmware roots are unchanged. The `Paths`
constructor allows isolated host tests to exercise both backends with temporary
files and simulated kernel operations, without FPGA hardware.
