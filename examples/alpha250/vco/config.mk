NAME := vco
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0
CORES += $(SDK_PATH)/fpga/cores/right_shifter_v1_0

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/vco.hpp

WEB_FILES += $(SDK_PATH)/web/index.html