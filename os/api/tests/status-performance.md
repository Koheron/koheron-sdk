# Live status polling on Red Pitaya

Measured on 2026-10-10 at `192.168.1.85` with the existing FFT instrument,
Ubuntu Base 26.04.1, Linux 6.18.0-xilinx and Python 3.14.

Each status request reads `koheron-server.service`'s `ActiveState` through the
existing `libsystemd` library, avoiding a `systemctl` child process. The helper
opens and closes a bus connection for each query, including the initial query
before uWSGI forks. It does not retain state or a connection between requests.
It accepts `active`, `reloading` and `refreshing`, matching
[systemctl's active-state test](https://github.com/systemd/systemd/blob/v259/src/systemctl/systemctl-is-active.c).

Native failures become `OSError`, and the API clears live status on an error.
One five-second deadline covers connection authentication and the property
query. The uWSGI unit wants and waits for `dbus.socket` so the system bus can
activate when needed during early startup. No additional rootfs package is
required. Extracted-file identity and version checks still run on every request.

## Hardware measurements

| HTTP endpoint | Original median / p95 | Captured systemctl output | Native query |
| --- | ---: | ---: | ---: |
| GET /api/instruments | 70.46 / 78.13 ms | 50.45 / 54.93 ms | 12.37 / 13.88 ms |
| GET /api/instruments/details | 70.31 / 76.76 ms | 50.48 / 57.62 ms | 10.80 / 13.81 ms |

The baseline API was from `aad13a89`. Captured output was an intermediate
implementation that removed Python's timed-wait polling delay but still spawned
systemctl. Each HTTP comparison used 40 requests per implementation and rotated
which ran first. Three temporary uWSGI instances used ports 18082–18084 and the
board's installed uWSGI settings. They served HTTP directly, without nginx, and
used the real instrument directory, systemd and journal. All responses were 200,
matched across implementations, and reported FFT's loaded version.
Median HTTP latency fell by 82–85% against the original implementation, and
75–79% against captured output. This is a short wall-time measurement, not a CPU,
throughput or RAM benchmark.

The final native helper also passed:

- 300 fresh queries: median 4.14 ms, p95 4.33 ms; descriptor count stayed at 5.
- Active, stopped and failed states using a temporary unrelated systemd unit.
- A deliberately silent Unix socket instead of the system bus: authentication
  timed out with `ETIMEDOUT` after 5.004 seconds. This test caught and corrected
  the prototype's reliance on method-call timeout alone, which did not bound
  authentication.

Earlier systemctl-only measurements (40 calls each) were 65.25 ms median with
inherited output and 45.95 ms with captured output. Those measurements were in
a separate run and are included in the raw data, rather than presented as an
interleaved native-query benchmark.

[Raw samples and response data](status-performance-red-pitaya.json) include
tested source hashes and cleanup checks. All three temporary API instances were
stopped. The production API hash was unchanged, FFT retained PID 147, and the
production nginx/uWSGI/instrument services remained active. The probe service
was stopped and its failed state reset.

## Host checks and limits

All 37 API regressions passed, including stopped/failed/rolled-back instruments,
missing identity/version files, native resource cleanup, authentication timeout
and query failure handling. Five rootfs settings tests and three systemd startup
tests passed. API staging copied the new helper with a matching SHA-256 hash;
`git diff --check` passed.

No OS image build, board reboot, production API deployment or new FPGA build was
performed. The board's older startup units are not evidence of current V1 boot
behavior. The additional `dbus.socket` ordering was checked with the host's
isolated systemd test units, not by booting a rebuilt image on the board.
