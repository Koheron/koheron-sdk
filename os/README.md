# OS image builds

Build and installation: [SDK quick start](../README.md#quick-start).

## Settings

Set `PASSWORD` and `TIMEZONE` in the environment before `make image` to override the defaults in [rootfs.mk](./rootfs.mk).

The image manifest records source tags separately from `vivado`, `vivado_build`,
`vitis` and `vitis_build`, queried from `VIVADO_PATH` and `VITIS_PATH` at packaging.
Unavailable values produce a warning and `unknown`. This identifies the selected
tools, not the history of cached artifacts; rebuild artifacts after changing tools.

## Tests

Image-build tests are in [tests/](./tests/); instrument loading tests are in [api/tests/](./api/tests/).

## Management web interface

The OS serves the management pages at `/koheron/`: installed instruments,
running status, server logs, system information and data rates. These pages
live in `os/www/` and share the instrument control styles in
`web/instrument/instrument.css`. Their assets ship with the OS image. The single-page manager
shows installed instruments, logs and system details together. It supports
upload/run/remove feedback, live status updates and log pause, follow and download.

Build them with `make CFG=examples/alpha250/fft/config.mk www`. The output is
`tmp/www/`. Run the host regression suite after installing the dependencies
from `web/package.json`:

```sh
NODE_PATH=web/node_modules node --test os/www/tests/test_*.cjs
```

The suite uses simulated HTTP responses and does not control hardware. Verify
upload, run, removal and log streaming on a board before deploying an OS image.

The data-rate monitor converts server rates from bits/s to bytes/s, uses server
timestamps for its four-minute history, and marks unchanged snapshots as stale.
Monitor tests cover scaling, unit conversions, polling lifecycle and stale data.
