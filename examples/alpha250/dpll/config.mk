NAME := dpll
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/alpha250

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_lfsr_v1_0
CORES += $(SDK_PATH)/fpga/cores/double_saturation_v1_0
CORES += $(SDK_PATH)/fpga/cores/boxcar_filter_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_unwrapper_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

include $(SDK_PATH)/boards/alpha250/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/dpll.hpp
DRIVERS += $(PROJECT_PATH)/dma.hpp

WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))