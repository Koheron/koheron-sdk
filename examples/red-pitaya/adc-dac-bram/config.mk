NAME := adc-dac-bram
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/config/clocks.xdc

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk
CORES += $(PROJECT_PATH)/address_counter_v1_0

DRIVERS += $(SDK_PATH)/server/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/adc_dac_bram.hpp

WEB_FILES += $(SDK_PATH)/web/index.html