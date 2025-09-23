NAME := adc-dac-bram
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

CORES += $(PROJECT_PATH)/address_counter_v1_0

include $(BOARD_PATH)/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc_dac_bram.hpp

WEB_FILES += $(SDK_PATH)/web/index.html