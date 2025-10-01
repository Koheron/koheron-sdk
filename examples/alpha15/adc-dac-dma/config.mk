NAME := adc-dac-dma
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/alpha15

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha15/config/ports.xdc

include $(SDK_PATH)/boards/alpha15/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/comparator_v1_0
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(SDK_PATH)/fpga/cores/bus_multiplexer_v1_0

include $(SDK_PATH)/boards/alpha15/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc-dac-dma.hpp

WEB_FILES += $(SDK_PATH)/web/index.html