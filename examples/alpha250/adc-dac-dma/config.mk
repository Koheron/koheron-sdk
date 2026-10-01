NAME := adc-dac-dma
VERSION := 0.2.1

BOARD_PATH := $(SDK_PATH)/boards/alpha250

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/bus_multiplexer_v1_0
CORES += $(PROJECT_PATH)/axis_packetizer_v1_0
CORES += $(PROJECT_PATH)/reset_pulser_v1_0
CORES += $(PROJECT_PATH)/axis_trig_gate_v1_0

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/adc-dac-dma.hpp
DRIVERS += $(PROJECT_PATH)/adc-dac-dma.cpp

WEB_FILES += $(SDK_PATH)/web/index.html
