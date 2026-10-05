NAME := phase-noise-analyzer
VERSION := 1.1.0
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
CORES += $(SDK_PATH)/fpga/cores/phase_unwrapper_v1_0
CORES += $(SDK_PATH)/fpga/cores/phase_prefilter_v1_0
CORES += $(SDK_PATH)/fpga/cores/axis_variable_v1_0

CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
TCL_FILES = $(BD_TCL) $(PROJECT_PATH)/post_route.tcl $(wildcard $(PROJECT_PATH)/tcl/*.tcl) $(wildcard $(BOARD_PATH)/*.tcl) $(BOARD_PATH)/config/board_preset.tcl $(wildcard $(FPGA_PATH)/lib/*.tcl) $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl

DRIVERS += $(BOARD_PATH)/drivers/common.hpp
DRIVERS += $(PROJECT_PATH)/phase-modulator.hpp
DRIVERS += $(SDK_PATH)/server/drivers/dma-s2mm.hpp
DRIVERS += $(PROJECT_PATH)/dds.hpp
DRIVERS += $(PROJECT_PATH)/dds.cpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.hpp
DRIVERS += $(PROJECT_PATH)/phase-noise-analyzer.cpp
include $(SDK_PATH)/server/drivers/phase-noise/fft.mk

# Share the ALPHA250 analyzer workspace, with board-specific entry points.
PNA_WEB_REFERENCE := $(SDK_PATH)/examples/alpha250/phase-noise-analyzer/web
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/frequency-input.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator-widget.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.css
WEB_FILES += $(SDK_PATH)/web/phase-noise/phase-precision.ts
WEB_FILES += $(SDK_PATH)/web/phase-noise/phase-precision.css
WEB_FILES += $(SDK_PATH)/web/jquery.flot.d.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.ts
WEB_FILES += $(SDK_PATH)/web/plot-basics/plot-basics.html
WEB_FILES += $(PNA_WEB_REFERENCE)/dds.ts
WEB_FILES += $(PNA_WEB_REFERENCE)/phase-noise-analyzer.ts $(PNA_WEB_REFERENCE)/phase-noise-analyzer-app.ts
WEB_FILES += $(PNA_WEB_REFERENCE)/plot.ts $(PNA_WEB_REFERENCE)/phase-noise-plot.css $(PNA_WEB_REFERENCE)/phase-noise.css
WEB_FILES += $(wildcard $(PNA_WEB_REFERENCE)/dds-frequency/* $(PNA_WEB_REFERENCE)/export-file/*)
WEB_FILES += $(wildcard $(PROJECT_PATH)/web/*)

OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi
