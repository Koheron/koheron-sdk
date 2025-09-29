
BOARD := red-pitaya
PART := xc7z010clg400-1
ZYNQ_TYPE := zynq

# Linux and U-boot
TMP_OS_BOARD_PATH := $(TMP)/red-pitaya

UBOOT_CONFIG = zynq_red_pitaya_defconfig
UBOOT_TAG := xilinx-uboot-v$(VIVADO_VERSION)
DTREE_TAG := xilinx_v$(VIVADO_VERSION)

UBOOT_URL := https://github.com/Xilinx/u-boot-xlnx/archive/xilinx-v$(VIVADO_VERSION).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/refs/tags/$(DTREE_TAG).tar.gz

PATCHES := boards/red-pitaya/patches