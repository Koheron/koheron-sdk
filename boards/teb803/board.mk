# Linux and U-boot
UBOOT_CONFIG = zynq_mercury_ux1_defconfig 
DTREE_OVERRIDE = arch/arm64/boot/dts/xilinx/zynqmp-enclustra-xu1.dtb
DTREE_LOC = linux  
UBOOT_TAG := enclustra-uboot-v$(VIVADO_VERSION)
LINUX_TAG := enclustra-linux-v$(VIVADO_VERSION)
DTREE_TAG := xilinx-v$(VIVADO_VERSION)
ZYNQ_TYPE := zynqmp

UBOOT_URL := https://github.com/enclustra-bsp/xilinx-uboot/archive/v1.6.1.tar.gz
LINUX_URL := https://github.com/enclustra-bsp/xilinx-linux/archive/v1.6.1.tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/$(DTREE_TAG).tar.gz
