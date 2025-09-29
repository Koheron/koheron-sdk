BOARD := alpha250-1g
PART := xc7z020clg400-2
ZYNQ_TYPE := zynq

# Linux and U-boot
TMP_OS_BOARD_PATH := $(TMP)/alpha250-1g

UBOOT_CONFIG = zynq_alpha250-1g_defconfig
UBOOT_TAG := xilinx-uboot-v$(VIVADO_VERSION)
DTREE_TAG := xilinx_v$(VIVADO_VERSION)

UBOOT_URL := https://github.com/Xilinx/u-boot-xlnx/archive/xilinx-v$(VIVADO_VERSION).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/refs/tags/$(DTREE_TAG).tar.gz

FSBL_PATH := $(OS_PATH)/alpha/fsbl

PATCHES := boards/alpha250-1g/patches
