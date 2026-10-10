# Native management runtime tests

[Deployed management verification](native-management-deployed-red-pitaya.json)
records the current API and UI installed at the board's regular port-80 URL,
`http://192.168.1.85/koheron/`. The earlier repeat tests below used a private
preview listener and removed its services afterward; they did not update that
URL. This deployment installs the runtime libraries, native binaries, service
units, nginx configuration and web assets, and disables the old uWSGI service.
All 30 installed files match the build. API/socket/nginx are enabled for boot;
the board was not rebooted and a complete OS image was not flashed.

The [browser record](../../www/tests/native-management-deployed-red-pitaya-browser.json)
covers nine read-only checks against the real running FFT: status, journal and
health, a 12-second WebSocket connection without HTTP fallback, compatibility,
command details, live transfer rates, four viewport widths and the FFT's live
spectrum over its instrument WebSocket connection. There are no
JavaScript errors, failed resource requests or horizontal overflow. Reviewed
screenshots: [desktop](../../www/tests/native-management-deployed-red-pitaya-desktop.png)
and [mobile](../../www/tests/native-management-deployed-red-pitaya-mobile.png),
plus [the live FFT](../../www/tests/native-management-deployed-red-pitaya-fft.png).
The FFT and nginx PIDs remain 151 and 399. TCP reads still return 2048 points,
1023 averages and Hann window; no instrument lifecycle action or FPGA programming
was performed. The old UI/API/configuration are retained on the board at
`/var/lib/koheron-management-backup-20261010`.

The user's existing Chrome initially failed on the FFT page: nginx returned
403 for `jquery.min.js`, whose archive permissions were 0600. Bootstrap's JS
and CSS had the same permissions. `download_verified.sh` now publishes verified
public inputs as 0644 and repairs verified cached files. Three regression tests
cover fresh downloads under a restrictive umask, cached permission repair
without a download, and preserving the previous file on checksum failure.
The board's three live assets and stored FFT archive permissions were repaired;
all 41 archive payload hashes remain unchanged. After reloading the user's
Chrome, the live spectrum renders at 56–58 FPS with Hann and 2,048 points.

To repeat the deployed FFT 0.3.0 UI verification without instrument mutations:

```sh
MANAGEMENT_URL=http://BOARD/koheron/ \
CHROMIUM_PATH=/path/to/chromium \
node os/www/tests/management_readonly_browser.cjs
```

`PUPPETEER_MODULE` can select Puppeteer Core outside Node's default search path;
`BROWSER_OUTPUT` selects the record and screenshot directory.

[Full Red Pitaya repeat validation](native-management-red-pitaya-retest-validation.json)
tests the polished application at `dc70fcc3`: 27 native integration checks and
16 Chromium checks, including UI boot selection, activation, ZIP upload/removal,
compatibility rejection, failed-start rollback and diagnostics. The [raw board
record](native-management-red-pitaya-retest.json), [browser record](../../www/tests/native-management-red-pitaya-retest-browser.json)
and [browser output](../../www/tests/native-management-red-pitaya-retest-browser.txt)
preserve the results. The ARMhf API/UI build targets and doctor pass; unchanged
artifacts match the polish snapshot. A [read-only production FFT probe](native-management-red-pitaya-retest-fft.json)
matches before the UI run and after cleanup: FFT 0.3.0, server 1.0, 2048 points,
1023 averages and Hann window. Production PIDs/API hash remain unchanged,
and private units/staging are removed. No FPGA programming, image deployment,
reboot or extended stress run is involved. Reviewed screenshots:
[desktop](../../www/tests/native-management-red-pitaya-retest-desktop.png),
[mobile](../../www/tests/native-management-red-pitaya-retest-mobile.png).

The extended browser checks use the full `red_pitaya_integration.py
--management-controls --hardening --preview` fixtures. Add `scope` and
`wide-spectrum-analysis` with the harness's `package()`/`upload()` helpers, then
set `MANAGEMENT_ACTION_TESTS=1` and `MANAGEMENT_UPLOAD_FIXTURE=/path/to/ui-upload.zip`
when invoking `management_polish_browser.cjs`. Generate that dummy ZIP with
`package('ui-upload')`; it is uploaded and removed only through the private API.
For the separate production probe, run on the host:

```sh
PYTHONPATH=python .venv/bin/python os/api/tests/red_pitaya_readonly_probe.py \
  --host BOARD --output /path/to/fft-probe.json
```

[UI polish validation](../../www/tests/native-management-polish-validation.json)
records 36 browser regressions, the TypeScript/web build, and a targeted Chromium
run against private services on the physical Red Pitaya. It covers readable
lifecycle feedback, manual connection retry, action-menu dismissal, keyboard
focus, command-load retry, and 320/390/768/1360px layouts. Startup durations now
retain millisecond precision; small packages retain byte precision. The native
binaries are unchanged from the hardening snapshot below.
[Board cleanup](native-management-polish-red-pitaya.json) and
[console output](native-management-polish-red-pitaya.txt) record production PID
and API preservation. The [browser record](../../www/tests/native-management-polish-browser.json)
separates real API interactions from request failures simulated in Chromium.
Reviewed screenshots: [desktop](../../www/tests/native-management-polish-desktop.png),
[mobile](../../www/tests/native-management-polish-mobile.png),
[mobile actions](../../www/tests/native-management-polish-mobile-menu.png),
[details](../../www/tests/native-management-polish-details.png).

For a short UI preview, stage `red_pitaya_ui_preview.py` alongside
`red_pitaya_integration.py`, the private binaries, configs, libraries and `www/`.
Run it with `--preview`; it serves four dummy instruments for up to three
minutes, then removes its services. Touch `/tmp/native-management/preview-done`
to finish early. Run the host browser checks with Puppeteer Core and Chromium:

```sh
MANAGEMENT_URL=http://BOARD:18087/koheron/ \
CHROMIUM_PATH=/path/to/chromium \
node os/www/tests/management_polish_browser.cjs
```

`PUPPETEER_MODULE` can select an installed module outside the default Node search
path; `BROWSER_OUTPUT` selects the screenshot/record directory. Copy the board's
`integration.json` and `preview.txt` before removing its staging directory.
This preview does not run the native integration or stress suites.

[Hardening validation](native-management-hardening-validation.json) records the
preceding follow-up checks and artifact hashes: 109 native tests, 101 sanitizer
tests, 26 browser regressions, and ARMhf/ARM64 builds. The suite covers foreign
browser requests, navigation/HEAD side effects, reflected HTML, strict metadata,
idle WebSocket expiration, late HTTP results, cancellation, fallback recovery,
and validation before replacing the boot-default archive.
The [hardening board record](native-management-hardening-red-pitaya.json) and
[console output](native-management-hardening-red-pitaya.txt) cover 27 private
fixture checks, including the browser boundary, proxy port preservation,
heartbeat expiration, default replacement, and the POST activation log bookmark.
[Hardening Chromium results](../../www/tests/native-management-hardening-browser.json)
record desktop/mobile controls and a stable heartbeat without HTTP fallback.
The earlier extension measurements below refer to the original implementation
snapshot; their hashes are preserved.

[Management validation](native-management-validation.json) records source and
binary hashes for the API/UI extension: 100 native tests, 19 browser regressions,
ARMhf/ARM64 builds and 92 sanitizer tests. The new cases cover compatibility,
staging capacity, lifecycle controls, boot selection, durability failures and
the bounded WebSocket protocol/connection lifecycle.

[Red Pitaya results](native-management-red-pitaya.json) and
[console output](native-management-red-pitaya.txt) record 19 checks through
private systemd notify services and a separate nginx listener on the physical
board. `red_pitaya_integration.py --management-controls` selects these cases;
add `--hardening` for the browser request boundary and heartbeat checks.
`--preview` temporarily serves staged management assets for browser inspection.
The harness runs on the existing laboratory image with Python available; it is
not part of the standard board runtime. It leaves the production FFT server,
nginx and API unchanged, removes its units and performs no FPGA/acquisition,
SD flashing, reboot or extended stress test.

[Browser results](../../www/tests/native-management-browser.json) record real
Chromium Stop/Start, preflight and viewport checks against that private board
listener, with no page errors or horizontal overflow at 390 pixels. Reviewed
screenshots: [desktop](../../www/tests/native-management-desktop.png),
[preflight](../../www/tests/native-management-preflight.png),
[mobile](../../www/tests/native-management-mobile.png).

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
Boot extraction tests exercise `koheron-install --extract-default` without
service control, including both loader formats, CRC checks of omitted reference
bitstreams, default-name/path validation, replacement and file-swap recovery.
[Native boot validation](native-boot-validation.json) records the focused checks,
cross-build hashes and extraction from the assembled ARM image in QEMU.

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
    for suite in api install boot led activation management hardening; do
      python3 -m unittest discover -s os/api/tests -p "test_native_$suite.py" -v
    done
  '
```

Check that no sanitizer diagnostic files are created: several invalid-input
tests intentionally expect nonzero exits and capture the child process's stderr.
The eight host fault-injection tests use a separate preload shim and are covered
by the ordinary Docker suite.
