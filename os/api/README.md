# Board management

A V1 image serves its management pages at `http://<board-ip>/koheron/`.
They show installed instruments, running status, logs and system information,
and support upload, activation and service controls. The
[development guide](../../docs/development.md#deployment-and-logs) covers the
usual `make deploy`, `status` and `logs` commands.

## HTTP API

The existing host SDK routes remain compatible. These endpoints are available
for clients that need direct access:

| Method | Route | Purpose |
| --- | --- | --- |
| GET | `/api/instruments/preflight/NAME` | Check an archive before activation. |
| POST | `/api/instruments/activate/NAME` | Activate an installed instrument and wait for readiness. |
| POST | `/api/instruments/control/start` | Start the loaded instrument. |
| POST | `/api/instruments/control/stop` | Stop the loaded instrument. |
| POST | `/api/instruments/control/restart` | Restart the loaded instrument. |
| POST | `/api/instruments/default/NAME` | Set the instrument to load at boot. |
| GET | `/api/system/status` | Read instruments, current operation and board health. |
| GET | `/api/system/diagnostics` | Download status and diagnostic logs. |
| WebSocket | `/api/events` | Follow status and operation changes. |
| WebSocket | `/api/logs/koheron/events?cursor=CURSOR` | Follow instrument logs. |
| GET | `/api/logs/koheron/tail?cursor=CURSOR` | Read a log batch over HTTP. |

## Installation and compatibility

New instrument archives include `instrument.json` with board and runtime
requirements. Preflight checks the archive and board compatibility before
activation; older archives remain usable with a warning. These checks cannot
prove that an FPGA design or driver will work on hardware.

Activation stages the new instrument, waits for service readiness and attempts
to restore the previous installation on failure. The new mutation endpoints use
POST; an operation in progress rejects another mutation with HTTP 409. Failure
responses include `code`, `error` and a `rollback` result. Uploads are limited
to 20 MiB.

## Status and logs

`/api/system/status` reports the installed and loaded instruments, the current
or last operation, and board health. The status WebSocket sends updates when
operations change and periodic snapshots.

The home page also displays power, voltage, current and temperature from
`health.telemetry`, using the same status WebSocket and HTTP fallback. An optional
board service publishes `/run/koheron/telemetry.json` independently of the
instrument. Missing readings appear as a dash; snapshots older than five seconds
are marked stale. Stopping an instrument does not stop board monitoring.

The producer atomically replaces this UTF-8 JSON file (maximum 16 KiB):

```json
{"sampled_monotonic_us":123456789,"supplies":[{"label":"Input","voltage_v":12.0,"current_a":0.5,"power_w":6.0}],"temperatures":[{"label":"Processor","celsius":42.0}]}
```

`sampled_monotonic_us` uses `CLOCK_MONOTONIC` on the same running system. Each
of the supply and temperature arrays accepts up to sixteen channels with labels
of at most eighty UTF-8 bytes.
An optional `voltages` array holds up to thirty-two voltage-only channels, each
with `label` and `voltage_v` fields. The home page shows them under **Supply
voltages**, without implying a current or power measurement.
Unavailable measurements are `null`. The management API only reads this snapshot;
board services own sensor access and recovery. It never loads an FPGA or starts
an instrument to collect telemetry. `--telemetry PATH` selects a fixture or a
different producer path.

An optional `statuses` array carries up to sixteen diagnostic channels. Each
has a `label` (up to eighty UTF-8 bytes), `state` (`ok`, `warning`, `fault`, or
`unavailable`) and a `message` (up to 512 UTF-8 bytes). Invalid states become
`unavailable`. These channels share the snapshot's freshness and connection
indicators. Status messages are displayed as text; sensor-specific decoding
belongs to the producer.

System health also includes `cpu.policies`, discovered under
`/sys/devices/system/cpu/cpufreq/policy*`, with up to thirty-two policies. Each
contains `label`, `affected_cpus`, `governor`, `frequency_hz` and
`frequency_source`. The reader prefers `cpuinfo_cur_freq` (`hardware`) and
falls back to `scaling_cur_freq` (`requested`), converting kHz to Hz. The latter
can represent the driver's last requested P-state rather than a measured clock.
Missing or invalid frequency readings remain `null`; systems without CPUFreq
policies return an empty array. `--sys PATH` selects a sysfs fixture root.
The reader never changes governors or scaling settings. CPU frequency, existing
1/5/15-minute load averages, memory, storage and uptime use the same management
status WebSocket, even with no instrument running. See the
[Linux CPUFreq ABI](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html).

Log entries include microsecond timestamps, message text, priority and a
truncation flag. The WebSocket and HTTP log endpoints share cursors, so clients
can resume after reconnecting or fall back to HTTP. An expired cursor returns
`reset: true` with recent history.
