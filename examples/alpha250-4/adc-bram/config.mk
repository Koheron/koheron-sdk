NAME := adc-bram
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250-4

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250-4/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/examples/alpha250/adc-dac-bram/address_counter_v1_0

include $(SDK_PATH)/boards/alpha250-4/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc_bram.hpp

WEB_FILES += $(SDK_PATH)/web/index.html