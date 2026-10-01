NAME := phase-noise-analyzer
VERSION := 1.0.0

BOARD_PATH := $(SDK_PATH)/boards/alpha250

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250/config/ports.xdc

include $(BOARD_PATH)/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/tlast_gen_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_lfsr_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_unwrapper_v1_0
CORES += $(SDK_PATH)/fpga/cores/boxcar_filter_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

include $(BOARD_PATH)/drivers/drivers.mk
DRIVERS += $(SDK_PATH)/server/drivers/dma-s2mm.hpp
DRIVERS += $(PROJECT_PATH)/dds.hpp
DRIVERS += $(PROJECT_PATH)/dds.cpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.hpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.cpp

# Web assets
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))

OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi