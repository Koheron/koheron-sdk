NAME := signal_analyzer
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/alpha15

CONFIG_YML = $(PROJECT_PATH)/config.yml

XDC += $(SDK_PATH)/boards/alpha15/config/ports.xdc

include $(SDK_PATH)/boards/alpha15/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/right_shifter_v1_0

include $(SDK_PATH)/boards/alpha15/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/vco.hpp

WEB_FILES += $(SDK_PATH)/web/index.html