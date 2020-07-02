PROC := psu_cortexa53_0
GCC_FLAGS := -march=armv8-a -mcpu=cortex-a53
GCC_ARCH := aarch64-linux-gnu
LINUX_IMAGE := Image
ARCH := arm64

BOOTCALL := $(TMP_OS_PATH)/bootmp.bin

ARMTRUST_URL := https://github.com/Xilinx/arm-trusted-firmware/archive/xilinx-v$(VIVADO_VERSION).tar.gz
ATRUST_TAG := arm-trust-v$(VIVADO_VERSION)
ATRUST_PATH := $(TMP_OS_PATH)/a-trust-xlnx-$(ATRUST_TAG)
ATRUST_TAR := $(TMP)/arm-trust-xlnx-$(ATRUST_TAG).tar.gz