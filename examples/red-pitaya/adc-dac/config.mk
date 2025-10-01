NAME := adc-dac
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/config/clocks.xdc

include $(SDK_PATH)/boards/red-pitaya/cores/cores.mk

DRIVERS += $(SDK_PATH)/server/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/adc_dac.hpp

WEB_FILES += $(SDK_PATH)/web/index.html
