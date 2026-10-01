NAME := irq
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(PROJECT_PATH)/constraints.xdc

SERVERLESS_CPP_SRCS := $(PROJECT_PATH)/main.cpp
SERVER_MK = $(SDK_PATH)/server/serverless.mk

WEB_FILES += $(SDK_PATH)/web/index.html

OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi
