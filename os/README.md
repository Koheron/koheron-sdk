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
