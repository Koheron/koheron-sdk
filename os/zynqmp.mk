PROC := psu_cortexa53_0
# -cpu shorthand for -march + -mtune
# -mcpu=cortex-a53 <=> march=armv8-a+crc -mtune=cortex-a53
GCC_FLAGS := -mcpu=cortex-a53
GCC_ARCH := aarch64-linux-gnu
ARCH := arm64

BOOT_BIN := bootmp.bin

UBOOT_ARCH := arm
UBOOT_CFLAGS := -O2

ARMTRUST_URL := https://github.com/Xilinx/arm-trusted-firmware/archive/xilinx-v$(VIVADO_VERSION).tar.gz
ATRUST_TAG := arm-trust-v$(VIVADO_VERSION)
ATRUST_PATH := $(TMP_OS_PATH)/a-trust-xlnx-$(ATRUST_TAG)
ATRUST_TAR := $(TMP)/arm-trust-xlnx-$(ATRUST_TAG).tar.gz

KERNEL_BIN := Image

UBUNTU_ARCH := arm64
QEMU_BIN    := /usr/bin/qemu-aarch64-static