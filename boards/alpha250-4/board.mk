# Linux and U-boot
UBOOT_CONFIG = zynq_alpha250_defconfig  
ZYNQ_TYPE ?= zynq
VIVADO_VER = 2020.1
VIVADO_VER2 = 2020.1
UBOOT_TAG := xilinx-uboot-v$(VIVADO_VER)
LINUX_TAG := xilinx-linux-v$(VIVADO_VERSION)
DTREE_TAG := xilinx-v$(VIVADO_VER)

UBOOT_URL := https://github.com/Xilinx/u-boot-xlnx/archive/xilinx-v2020.1.tar.gz
LINUX_URL := https://github.com/Xilinx/linux-xlnx/archive/xilinx-v$(VIVADO_VERSION).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/$(DTREE_TAG).tar.gz
