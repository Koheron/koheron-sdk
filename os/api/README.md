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
array accepts up to sixteen channels with labels of at most eighty UTF-8 bytes.
Unavailable measurements are `null`. The management API only reads this snapshot;
board services own sensor access and recovery. It never loads an FPGA or starts
an instrument to collect telemetry. `--telemetry PATH` selects a fixture or a
different producer path.

Log entries include microsecond timestamps, message text, priority and a
truncation flag. The WebSocket and HTTP log endpoints share cursors, so clients
can resume after reconnecting or fall back to HTTP. An expired cursor returns
`reset: true` with recent history.
