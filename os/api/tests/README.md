# Native management runtime tests

Build and run the black-box suite with the same Ubuntu 26.04/GCC 15 toolchain
used for board binaries:

```sh
docker build -f os/api/tests/Dockerfile -t koheron-management-tests .
docker run --rm -v "$PWD:/work" -w /work koheron-management-tests
```

Python is only the host test harness. It is not installed in the board image.
Tests launch the compiled HTTP daemon, installer and LED helper. They exercise
real TCP/Unix sockets, multipart streaming, ZIP CRC validation, permissions,
directory swaps and recovery. Service control uses a private executable fixture.
Filesystem failures are injected with a host-only shared library. Activation
tests pass real systemd-style descriptors and verify readiness and socket reuse.
LED tests exchange fragmented binary RPC messages with a real Unix peer.

The Docker test job also checks first-boot growth on a disposable disk image,
rootfs settings and enablement in a private chroot. No real disks are used.

The native migration's physical Red Pitaya tests and raw measurements are in
[native-runtime-evidence.md](native-runtime-evidence.md). Earlier Python API
status measurements remain in [status-performance.md](status-performance.md).

The loader validates/extracts before stopping the current service, retains its
files until the replacement reaches systemd readiness, and restores/restarts the
previous installation if startup fails. If file restoration itself fails, it
keeps the backup and prints its path for recovery. It cannot guarantee recovery
from process termination, power loss, or hardware that fails to accept the old
configuration; those require separate system-level testing.

The live identity marker is written by runtime installation and default extraction
at boot. After upgrading only the API on an already-running system, status is
unknown (`null`) until an instrument is reloaded or boot extraction writes the
marker. It never guesses from the boot default. The native API serializes upload,
installation and deletion while allowing concurrent status requests.

The opt-in `red_pitaya_benchmark.py` and `red_pitaya_integration.py` scripts run
on an existing Python-based V1 board after staging files under
`/tmp/native-management`. The benchmark compares the previous V1 Python API
with the native daemon on loopback port 18085. Integration uses temporary
systemd units, nginx on 18087 and dummy instrument payloads. It also exercises
the native LED RPC against the running FFT server. These scripts remove their
temporary processes and units without changing production services. They do
not flash an SD card or validate FPGA programming or acquisition.
