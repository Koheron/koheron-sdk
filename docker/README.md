# Build compilers

The default `cross-armhf:26.04` Docker builder supplies Ubuntu 26.04 and GCC 15
compilers for native, ARM hard-float and AArch64 builds. `make setup` builds it;
the reference host remains Ubuntu 24.04 with Vivado/Vitis 2025.1.

To rebuild only the Docker builder:

```sh
docker build -f docker/Dockerfile -t cross-armhf:26.04 docker
```

Server, kernel, U-Boot and ARM Trusted Firmware use GCC 15 by default. FSBL/PMU
firmware uses the bare-metal toolchains. Server, instrument and board management
code uses C++23; the standalone C++ client supports C++20. Compiler selection
is independent of the Vivado release.

`DOCKER_IMAGE` selects a custom builder. `HOST_GCC_VERSION`, `KERNEL_GCC_VERSION`,
`UBOOT_GCC_VERSION` and `ATF_GCC_VERSION` can override individual compiler
versions if the chosen image provides them.

Cross sysroot headers and libraries match the default V1 Ubuntu 26.04 rootfs.
