BOARD := alpha250
PART := xc7z020clg400-2
ZYNQ_TYPE := zynq

CORES += $(SDK_PATH)/fpga/cores/axi_ctl_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/axi_sts_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/edge_detector_v1_0
CORES += $(SDK_PATH)/fpga/cores/comparator_v1_0
CORES += $(SDK_PATH)/fpga/cores/unrandomizer_v1_0
CORES += $(SDK_PATH)/boards/alpha250/cores/precision_dac_v1_0
CORES += $(SDK_PATH)/boards/alpha250/cores/spi_cfg_v1_0

XDC := $(BOARD_PATH)/config/ports.xdc

DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/common.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/eeprom.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/gpio-expander.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/temperature-sensor.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/power-monitor.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/clock-generator.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/ltc2157.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/ad9747.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/precision-adc.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/precision-dac.hpp
DRIVERS += $(SDK_PATH)/boards/alpha250/drivers/spi-config.hpp

# Linux and U-boot
UBOOT_CONFIG = zynq_alpha250_defconfig
UBOOT_TAG := xilinx-uboot-v$(VIVADO_VERSION)
LINUX_TAG := xilinx-linux-v$(VIVADO_VERSION)
DTREE_TAG := xilinx_v$(VIVADO_VERSION)

UBOOT_URL := https://github.com/Xilinx/u-boot-xlnx/archive/xilinx-v$(VIVADO_VERSION).tar.gz
LINUX_URL := https://github.com/Xilinx/linux-xlnx/archive/refs/tags/xilinx-v$(VIVADO_VERSION).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/refs/tags/$(DTREE_TAG).tar.gz

FSBL_PATH := $(OS_PATH)/alpha/fsbl

PATCHES := boards/alpha250/patches
