# OS image builds

Build and installation: [SDK quick start](../README.md#quick-start).

## Settings

Set `PASSWORD` and `TIMEZONE` in the environment before `make image` to override the defaults in [rootfs.mk](./rootfs.mk).

The image ZIP includes `manifest-<board>-<instrument>.txt`, also installed at
`/usr/local/share/koheron/manifest.txt` in the image. Its `kernel`, `u-boot` and
`device-tree` fields identify source versions. The `vivado`, `vivado_build`,
`vitis` and `vitis_build` fields report the release and software build numbers
queried from the installations selected by `VIVADO_PATH` and `VITIS_PATH` when
the image is packaged. An unavailable or unrecognized tool reports `unknown`
and emits a warning. These fields do not track the provenance of reused build
artifacts; rebuild those artifacts when changing toolchains.

## Tests

Image-build tests are in [tests/](./tests/); instrument loading tests are in [api/tests/](./api/tests/).
