NAME := phase-noise-analyzer
VERSION := 1.3.2
ENFORCE_TIMING := 1

BOARD_PATH := $(SDK_PATH)/boards/alpha250-4

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(SDK_PATH)/boards/alpha250-4/config/ports.xdc

include $(SDK_PATH)/boards/alpha250/cores/cores.mk
CORES += $(SDK_PATH)/fpga/cores/axis_constant_v1_0
CORES += $(SDK_PATH)/fpga/cores/latched_mux_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_quantizer_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_lfsr_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_unwrapper_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_range_guard_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_stochastic_round_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_prefilter_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_stream_packet_mux_v1_0
CORES += $(PROJECT_PATH)/paired_cic_control_v1_0

include $(BOARD_PATH)/drivers/drivers.mk
DRIVERS += $(SDK_PATH)/server/drivers/dma-s2mm.hpp
DRIVERS += $(PROJECT_PATH)/dds.hpp
DRIVERS += $(PROJECT_PATH)/dds.cpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.hpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.cpp
include $(SDK_PATH)/server/drivers/phase-noise/fft.mk

# Web assets
include $(SDK_PATH)/web/phase-noise/components.mk
include $(SDK_PATH)/web/phase-noise/reference-clock/components.mk
WEB_FILES += $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))

# Board Tcl changes must invalidate the generated Vivado project as well.
TCL_FILES = $(BD_TCL) $(PROJECT_PATH)/post_route.tcl $(SDK_PATH)/fpga/lib/post_route_hold_fix.tcl $(SDK_PATH)/fpga/lib/pna_cordic.tcl $(wildcard $(PROJECT_PATH)/tcl/*.tcl) $(wildcard $(BOARD_PATH)/*.tcl) $(BOARD_PATH)/config/board_preset.tcl
TCL_FILES += $(SDK_PATH)/fpga/lib/pna_filter.tcl

OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi

# Full-precision split CIC and shared acquisition clock crossing.
CORES += $(SDK_PATH)/fpga/cores/phase_fixed_decimator_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_cic_decimator_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_stream_cdc_v1_0
