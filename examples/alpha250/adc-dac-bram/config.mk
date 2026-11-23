NAME := adc-dac-bram
VERSION := 0.2.2
ENFORCE_TIMING := 1

BOARD_PATH := $(SDK_PATH)/boards/alpha250
include $(SDK_PATH)/boards/alpha250/instrument.mk

FPGA_LIB_TCL = $(FPGA_TCL_BRAM)

CORES += $(PROJECT_PATH)/address_counter_v1_0
DRIVERS += $(PROJECT_PATH)/adc_dac_bram.hpp

WEB_FILES += $(SDK_PATH)/web/index.html
