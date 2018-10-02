# Linux and U-boot
UBOOT_CONFIG = zynq_red_pitaya_defconfig

UBOOT_TAG := koheron-red-pitaya-v$(VIVADO_VERSION)
LINUX_TAG := xilinx-v$(VIVADO_VERSION)
DTREE_TAG := xilinx-v$(VIVADO_VERSION)

UBOOT_URL := https://github.com/Koheron/u-boot-xlnx/archive/$(UBOOT_TAG).tar.gz
LINUX_URL := https://github.com/Xilinx/linux-xlnx/archive/$(LINUX_TAG).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/$(DTREE_TAG).tar.gz