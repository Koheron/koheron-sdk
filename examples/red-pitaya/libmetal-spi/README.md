# Libmetal SPI loopback

Run Xilinx's standalone `XSpi` driver inside a V1 Koheron instrument on the
original STEMlab 125-14. Libmetal opens the SPI controller through Linux UIO,
provides register access, and dispatches interrupts to `XSpi_InterruptHandler`.

**No additional hardware or wiring is required.** AXI Quad SPI is configured
in standard SPI mode and the driver enables its internal loopback. No SPI
signals are connected to board pins; the STARTUP interface is disabled.

```text
Python client → SDK RPC → XSpi driver → libmetal → UIO → AXI Quad SPI
                                                          TX ↔ RX
                                                      internal loopback
```

## Build

Use the SDK's [host setup](../../../README.md#requirements) and a **V1 OS image**
on the board. Build the example's compiler image once, from the SDK root:

```sh
docker build -f examples/red-pitaya/libmetal-spi/Dockerfile \
    -t koheron-libmetal-spi:2025.10 examples/red-pitaya/libmetal-spi
```

This extends `cross-armhf:26.04` with libmetal and the Xilinx SPI sources. The
example selects this image automatically. Libmetal and libsysfs are linked
statically, so no additional libraries need installing on the board.

```sh
make doctor CFG=examples/red-pitaya/libmetal-spi/stemlab-125-14.mk
make -j4 CFG=examples/red-pitaya/libmetal-spi/stemlab-125-14.mk N_CPUS=4
```

The instrument is packaged at
`tmp/examples/red-pitaya/libmetal-spi/stemlab-125-14/libmetal-spi.zip`.
Routed timing checks are enforced during the build.

## Run the hardware test

Substitute your board's address:

```sh
make deploy CFG=examples/red-pitaya/libmetal-spi/stemlab-125-14.mk HOST=192.168.1.100
make test CFG=examples/red-pitaya/libmetal-spi/stemlab-125-14.mk HOST=192.168.1.100
```

The test checks all-zero, all-one, incrementing and reproducible random data
at ten lengths from 1 to 65,536 bytes, including both sides of the 16-byte FIFO
boundary. Each transfer must return the exact transmitted bytes and produce
libmetal IRQ callbacks. A failure raises an exception and returns a nonzero
exit status. The output includes transfer duration and callback count; these
include Linux scheduling and FIFO refill overhead, and are not SPI wire-rate
measurements.

To experiment from this directory:

```python
from koheron import connect
from client import LibmetalSpi

client = connect("192.168.1.100", "libmetal-spi")
client.sock.settimeout(15)
try:
    spi = LibmetalSpi(client)
    if not spi.is_ready():
        raise RuntimeError(spi.get_error())
    print(spi.transfer([0x12, 0x34, 0xAB, 0xCD]))
finally:
    client.sock.close()
```

`transfer(data, timeout_ms=2000)` accepts 1..65,536 bytes and a completion timeout
of 1..10,000 ms. Device errors and timeouts stop/reset the controller and mark
the driver unavailable; restart the instrument before trying again. Invalid
arguments are rejected without changing the controller. Use a client socket
timeout longer than the requested SPI timeout.

## How the port works

- `block_design.tcl` connects an AXI Quad SPI controller to PS GP0, with its
  interrupt on the first PL-to-PS interrupt input. Both IP clocks use FCLK0.
- `memory.yml` defines the address, range, FIFO depth and clock settings shared
  by Tcl and C++. Its SDK memory-region list is empty because **libmetal owns
  the mapping**, including its descriptor and lifetime.
- `override.dtsi` binds the controller to `uio_pdrv_genirq`. The C++ code finds
  its platform-device name through the UIO physical address, so it does not
  depend on a particular `/dev/uioN` number. There is no `/dev/mem` fallback.
- `port/` and `port.cpp` replace the standalone BSP's register-access and timed
  wait functions. `Xil_In32`/`Xil_Out32` translate the driver's mapped addresses
  to libmetal region offsets and call `metal_io_read32`/`metal_io_write32`.
  `XSpi_Config` is supplied explicitly instead of using `xparameters.h` and a
  generated standalone configuration table.
- `libmetal_spi.cpp` registers the vendor ISR with libmetal. The vendor driver
  services the SPI FIFO and acknowledges the IP interrupt; libmetal acknowledges
  and re-arms UIO. Linux retains ownership of the interrupt controller.
- The SDK serializes RPC calls. The adapter also excludes the libmetal IRQ
  worker during driver setup, transfer submission, abort and destruction.
  It waits for the ISR to finish before releasing transfer buffers.

The example owns one libmetal runtime and one SPI device. Its port's mapping
slot is specific to that controller; supporting several independent controllers
would require an address-to-region registry and shared libmetal lifetime.

## Local checks

```sh
make check CFG=examples/red-pitaya/libmetal-spi/stemlab-125-14.mk
bash examples/red-pitaya/libmetal-spi/tests/run-host.sh
```

The host port test uses real libmetal with a RAM-backed I/O region. It checks
address translation, register access, timed waits, and rejection of invalid
accesses. Its three `invalid libmetal register access` messages are expected
child-process checks. It does not test FPGA behavior or Linux UIO delivery.
Report these checks separately from `make test` on a board.

## Upstream sources

The Dockerfile downloads unmodified sources at fixed commits:

- [libmetal v2025.10.0 / 1.9.0](https://github.com/OpenAMP/libmetal/tree/dbf9cf5286b4d7d44b930344398045916ebe8b8b),
  BSD-3-Clause; the source archive is also SHA-256 checked.
- [Xilinx embeddedsw 2025.1](https://github.com/Xilinx/embeddedsw/tree/c0aed2eff7a30f307238ec853fa8fbc45dcabdda/XilinxProcessorIPLib/drivers/spi),
  MIT; only the required SPI sources and portable BSP headers are downloaded.

Their licenses are retained under `/opt/libmetal` and `/opt/xilinx-spi` in the
builder. Libsysfs is provided by Ubuntu's `libsysfs-dev` packages.
