NAME := ram-writer
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(PROJECT_PATH)/constraints.xdc

CORES += $(SDK_PATH)/fpga/cores/axi_ctl_register_v1_0
CORES += $(SDK_PATH)/fpga/cores/axi_sts_register_v1_0
CORES += $(PROJECT_PATH)/axis_ram_writer_v1_0

SERVERLESS_CPP_SRCS := $(PROJECT_PATH)/main.cpp
SERVER_MK = $(SDK_PATH)/server/serverless.mk

WEB_FILES += $(SDK_PATH)/web/index.html