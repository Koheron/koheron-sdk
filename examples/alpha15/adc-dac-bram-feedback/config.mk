NAME := adc-dac-bram-feedback
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/alpha15

CONFIG_YML = $(PROJECT_PATH)/config.yml

XDC += $(SDK_PATH)/boards/alpha15/config/ports.xdc

include $(SDK_PATH)/boards/alpha15/cores/cores.mk
CORES += $(PROJECT_PATH)/address_counter_v1_0
CORES += $(PROJECT_PATH)/integrator_v1_0

include $(SDK_PATH)/boards/alpha15/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc_dac_bram_feedback.hpp

WEB_FILES += $(SDK_PATH)/web/index.html