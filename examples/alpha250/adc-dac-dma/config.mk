NAME := adc-dac-dma
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

CONFIG_YML = $(PROJECT_PATH)/config.yml

CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(SDK_PATH)/fpga/cores/bus_multiplexer_v1_0

include $(BOARD_PATH)/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc-dac-dma.hpp

WEB_FILES += $(SDK_PATH)/web/index.html