# Linux and U-boot
ZYNQ_TYPE ?= zynq
UBOOT_TAG := koheron-v$(VIVADO_VERSION)
LINUX_TAG := koheron-v$(VIVADO_VERSION)-kernel-module-fix-dma
DTREE_TAG := xilinx-v$(VIVADO_VERSION)

UBOOT_URL := https://github.com/Koheron/u-boot-xlnx/archive/$(UBOOT_TAG).tar.gz
LINUX_URL := https://github.com/Koheron/linux-xlnx/archive/$(LINUX_TAG).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/$(DTREE_TAG).tar.gz

FSBL_PATH := $(OS_PATH)/alpha/fsbl
