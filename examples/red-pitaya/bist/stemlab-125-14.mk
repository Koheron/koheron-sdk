NAME := bist
VERSION := 1.2.0
BOARD_PATH := $(SDK_PATH)/boards/red-pitaya/stemlab-125-14
ENFORCE_TIMING := 1
MEMORY_YML := $(PROJECT_PATH)/memory.yml
FPGA_LIB_TCL = $(FPGA_TCL_BASE) $(FPGA_TCL_REDP) $(FPGA_TCL_BRAM)
XDC += $(BOARD_PATH)/config/ports.xdc $(BOARD_PATH)/config/clocks.xdc
include $(BOARD_PATH)/cores/cores.mk
CORES += $(PROJECT_PATH)/bist_uart_v1_0 $(PROJECT_PATH)/bist_path_v1_0
OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi
DRIVERS += $(BOARD_PATH)/drivers/common.hpp $(PROJECT_PATH)/bist.hpp
DRIVERS += $(PROJECT_PATH)/bist.cpp
WEB_FILES += $(PROJECT_PATH)/web/index.html $(PROJECT_PATH)/web/bist.js $(PROJECT_PATH)/web/bist.css
WEB_FILES += $(PROJECT_PATH)/web/bist-metadata.js

# Record the exact reference implementation so OS comparisons also detect
# driver/workload changes made without a release-version bump.
BIST_SOURCE_FILES := $(sort $(wildcard $(PROJECT_PATH)/*.hpp $(PROJECT_PATH)/*.cpp $(PROJECT_PATH)/*_v1_0/*.v))
BIST_BUILD_ID := $(shell cat $(foreach F,$(BIST_SOURCE_FILES),$(call build_shell_quote,$(F))) | sha256sum | cut -d' ' -f1)
SERVER_EXTRA_CCXXFLAGS += -DBIST_BUILD_ID=\"$(BIST_BUILD_ID)\"
