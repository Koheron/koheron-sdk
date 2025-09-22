NAME := fft
VERSION := 0.2.1

CONFIG = $(PROJECT_PATH)/config.yml

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

XDC := $(BOARD_PATH)/config/ports.xdc

CORES += $(SDK_PATH)/fpga/cores/axi_ctl_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/axi_sts_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/edge_detector_v1_0
CORES += $(SDK_PATH)/fpga/cores/comparator_v1_0
CORES += $(SDK_PATH)/fpga/cores/unrandomizer_v1_0
CORES += $(SDK_PATH)/fpga/cores/psd_counter_v1_0

DRIVERS := $(SDK_PATH)/boards/alpha250/drivers/common.hpp
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
DRIVERS += $(PROJECT_PATH)/fft.hpp