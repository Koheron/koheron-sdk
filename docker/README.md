# Build compilers

The default `cross-armhf:26.04` builder uses Ubuntu 26.04 LTS and its packaged
GCC 15 native, ARM hard-float and AArch64 compilers. `make setup` installs the
packages in Docker; no GCC source build or newer host OS is required.
The reference development host remains Ubuntu 24.04 with Vivado/Vitis 2025.1.

To rebuild only the Docker builder:

```sh
docker build -f docker/Dockerfile -t cross-armhf:26.04 docker
```

Server, kernel, U-Boot and ARM Trusted Firmware use GCC 15 by default.
FSBL/PMU firmware keeps the existing bare-metal toolchains. Compiler selection
remains independent of the selected Vivado release.

`DOCKER_IMAGE` selects a custom builder. `HOST_GCC_VERSION`, `KERNEL_GCC_VERSION`,
`UBOOT_GCC_VERSION` and `ATF_GCC_VERSION` can override individual compiler
versions if the chosen image provides them.

Cross sysroot headers/libraries match Ubuntu 26.04, the default V1 rootfs.
The image includes native and ARMhf/ARM64 management libraries for HTTP, ZIP,
JSON, Unicode normalization and systemd. Server objects/PCH and OS outputs track compiler and Docker
image changes. The management runtime has no GCC 13 fallback.

Report build checks and physical boot/acquisition tests separately.
