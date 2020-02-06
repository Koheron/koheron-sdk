PROC := psu_cortexa53_0
# Arch flags obtain by running on the Zynq:
# gcc -march=native -Q --help=target
GCC_FLAGS := -march=armv8-a -mcpu=cortex-a53
GCC_ARCH := aarch64-linux-gnu
LINUX_IMAGE := Image
ARCH := arm64