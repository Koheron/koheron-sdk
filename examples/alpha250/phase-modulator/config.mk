NAME := phase-modulator
VERSION := 0.1.0
BOARD_PATH := $(SDK_PATH)/boards/alpha250
ENFORCE_TIMING := 1

XDC += $(BOARD_PATH)/config/ports.xdc
include $(BOARD_PATH)/cores/cores.mk
CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
TCL_FILES = $(BD_TCL) $(PROJECT_PATH)/post_route.tcl $(wildcard $(BOARD_PATH)/*.tcl) $(wildcard $(FPGA_PATH)/lib/*.tcl) $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl

include $(BOARD_PATH)/drivers/drivers.mk
DRIVERS += $(PROJECT_PATH)/phase_modulator.hpp
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/frequency-input.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator-widget.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.css
WEB_FILES += $(wildcard $(PROJECT_PATH)/web/*.ts $(PROJECT_PATH)/web/*.html $(PROJECT_PATH)/web/*.css)
