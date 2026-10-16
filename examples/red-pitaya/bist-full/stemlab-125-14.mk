NAME := bist-full
VERSION := 0.1.0
BOARD_PATH := $(SDK_PATH)/boards/red-pitaya/stemlab-125-14
ENFORCE_TIMING := 1
MEMORY_YML := $(PROJECT_PATH)/memory.yml
FPGA_LIB_TCL = $(FPGA_TCL_BASE) $(FPGA_TCL_REDP) $(FPGA_TCL_BRAM)
XDC += $(BOARD_PATH)/config/ports.xdc $(BOARD_PATH)/config/clocks.xdc
include $(BOARD_PATH)/cores/cores.mk
CORES += $(PROJECT_PATH)/../bist/bist_uart_v1_0 $(PROJECT_PATH)/full_path_v1_0
OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi
DRIVERS += $(BOARD_PATH)/drivers/common.hpp $(PROJECT_PATH)/full.hpp $(PROJECT_PATH)/completion.hpp
DRIVERS += $(PROJECT_PATH)/full.cpp
WEB_FILES += $(PROJECT_PATH)/web/index.html $(PROJECT_PATH)/web/full.js
FULL_SOURCES := $(sort $(wildcard $(PROJECT_PATH)/*.hpp $(PROJECT_PATH)/*.cpp $(PROJECT_PATH)/full_path_v1_0/*.v $(PROJECT_PATH)/../bist/*.hpp $(PROJECT_PATH)/../bist/bist_uart_v1_0/*.v))
FULL_BUILD_ID := $(shell cat $(foreach F,$(FULL_SOURCES),$(call build_shell_quote,$(F))) | sha256sum | cut -d' ' -f1)
SERVER_EXTRA_CCXXFLAGS += -DFULL_BUILD_ID=\"$(FULL_BUILD_ID)\"
