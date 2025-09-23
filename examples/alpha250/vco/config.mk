NAME := vco
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

XDC := $(BOARD_PATH)/config/ports.xdc

CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0
CORES += $(SDK_PATH)/fpga/cores/right_shifter_v1_0

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/vco.hpp

WEB_FILES += $(SDK_PATH)/web/index.html