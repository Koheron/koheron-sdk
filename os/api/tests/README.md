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

`red_pitaya_boot_evidence.py` is a separate, opt-in reboot harness. It stages
current V1 Python and native service variants on the existing rootfs, backs up
the touched configuration and installs a recovery timer. It requires persistent
staging under `/var/lib/koheron-native-boot-evidence` and serial/SSH access.
`boot_http_probe.cpp` independently records API readiness through nginx.
See the evidence report for staging, measured boot times, restoration and the
matched image-size comparison. This test reboots and temporarily changes the
production service configuration; it does not flash the generated image.

For an extended, opt-in LAN stress run, stage the three binaries, missing libraries under
`lib/`, the API service/socket, nginx configs and `red_pitaya_stress_board.py`
under `/tmp/koheron-native-stress`. Run that script's `setup` action on the
board, then run the host client:

```sh
python3 os/api/tests/red_pitaya_stress.py \
  --ssh-command 'ssh root@192.168.1.85' --output tmp/native-stress/load.json
```

The private frontend listens on `192.168.1.85:18089`; adjust the staged nginx
bind address and client/SSH arguments for another board. The default run ramps
through 1/8/32/64 clients, soaks for five minutes at 32, then mixes concurrent
readers with 30 activations, 10 failed-start rollbacks, uploads/deletes, 500
interrupted uploads, 50 restarts and three intentional SIGKILL recoveries.
The private fixture disables systemd start-rate limits to permit rapid test
restarts. It uses dummy executable payloads and never programs the FPGA.
Telemetry sampled about once per second includes API/nginx PSS, descriptors,
threads and CPU ticks; sampling work adds to the interval.
The host client's `finally` block stops private services and removes their units;
if the host process is terminated abruptly, run the board script's `cleanup`
action yourself. Copy `telemetry.jsonl` and `cleanup.json` before deleting the
staging directory. Production units/files remain untouched.

Host sanitizer checks use the same Docker image with a separate build directory:

```sh
docker run --rm -v "$PWD:/work" -w /work \
  -e NATIVE_API_BIN_DIR=tmp/native-api/asan \
  -e ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:log_path=/work/tmp/asan-report \
  -e UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:log_path=/work/tmp/ubsan-report \
  koheron-management-tests sh -ec '
    make -j2 -f os/api/Makefile CXX=g++-15 BUILD_DIR=$NATIVE_API_BIN_DIR \
      CXXFLAGS="-O1 -g -std=c++23 -Wall -Wextra -Wpedantic -Werror -pthread -MMD -MP -fsanitize=address,undefined -fno-omit-frame-pointer" \
      LDFLAGS="-fsanitize=address,undefined"
    for suite in api install led activation; do
      python3 -m unittest discover -s os/api/tests -p "test_native_$suite.py" -v
    done
    python3 os/api/tests/sanitizer_stress.py
  '
```

Check that no sanitizer diagnostic files are created: several invalid-input
tests intentionally expect nonzero exits and capture the child process's stderr.
The three host fault-injection tests use a separate preload shim and are covered
by the ordinary Docker suite.
