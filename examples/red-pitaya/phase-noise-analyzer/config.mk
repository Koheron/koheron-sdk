NAME := phase-noise-analyzer
VERSION := 1.3.2
ENFORCE_TIMING := 1

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/red-pitaya/config/ports.xdc
XDC += $(SDK_PATH)/boards/red-pitaya/config/clocks.xdc

include $(BOARD_PATH)/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_quantizer_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_lfsr_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_stochastic_round_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_unwrapper_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_prefilter_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
TCL_FILES = $(BD_TCL) $(PROJECT_PATH)/post_route.tcl $(wildcard $(PROJECT_PATH)/tcl/*.tcl) $(wildcard $(BOARD_PATH)/*.tcl) $(BOARD_PATH)/config/board_preset.tcl $(wildcard $(FPGA_PATH)/lib/*.tcl) $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl

DRIVERS += $(BOARD_PATH)/drivers/common.hpp
DRIVERS += $(BOARD_PATH)/drivers/phase-modulator.hpp
DRIVERS += $(PROJECT_PATH)/dds.hpp
DRIVERS += $(PROJECT_PATH)/dds.cpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.hpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.cpp
include $(SDK_PATH)/server/drivers/phase-noise/fft.mk

include $(SDK_PATH)/web/phase-noise/analyzer/workspace.mk
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))

OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi

CORES += $(SDK_PATH)/fpga/cores/axis_stream_packet_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_stream_control_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_range_guard_v1_0
