NAME := libmetal-spi
VERSION := 0.1.0
ENFORCE_TIMING := 1
BOARD_PATH := $(SDK_PATH)/boards/red-pitaya/stemlab-125-14

# Build this example's image first; see README.md.
DOCKER_IMAGE := koheron-libmetal-spi:2025.10
SERVER_MK := $(PROJECT_PATH)/server.mk
MEMORY_YML := $(PROJECT_PATH)/memory.yml
OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi
XDC += $(PROJECT_PATH)/constraints.xdc
DRIVERS += $(PROJECT_PATH)/libmetal_spi.hpp
DRIVERS += $(PROJECT_PATH)/libmetal_spi.cpp
DRIVERS += $(PROJECT_PATH)/port.cpp
WEB_FILES += $(SDK_PATH)/web/index.html

SERVER_EXTRA_CCXXFLAGS += -isystem /opt/libmetal/include -isystem /opt/xilinx-spi
SERVER_EXTRA_CCXXFLAGS += -I$(PROJECT_PATH)/port
