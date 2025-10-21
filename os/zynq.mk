PROC := ps7_cortexa9_0
# Arch flags obtain by running on the Zynq:
# gcc -march=native -Q --help=target
GCC_FLAGS := -march=armv7-a -mfpu=vfpv3-d16 -mvectorize-with-neon-quad -mfloat-abi=hard
GCC_ARCH := arm-linux-gnueabihf
ARCH := arm

UBOOT_ARCH := $(ARCH)
UBOOT_CFLAGS := -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard

BOOT_BIN := boot.bin

KERNEL_BIN := zImage
