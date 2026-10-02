NAME := adc-dma-chirp
VERSION := 0.1.0
BOARD_PATH := $(SDK_PATH)/boards/alpha250
ENFORCE_TIMING := 1

XDC += $(BOARD_PATH)/config/ports.xdc
include $(BOARD_PATH)/cores/cores.mk
CORES += $(PROJECT_PATH)/chirp_generator_v1_0
CORES += $(PROJECT_PATH)/adc_capture_v1_0

include $(BOARD_PATH)/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc_dma.hpp
WEB_FILES += $(SDK_PATH)/web/index.html
