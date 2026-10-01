NAME := test
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/kria-kr260

XDC += $(SDK_PATH)/boards/kria-kr260/config/default.xdc
XDC += $(PROJECT_PATH)/pmod.xdc

MEMORY_YML = $(PROJECT_PATH)/memory.yml

SERVERLESS_CPP_SRCS := $(PROJECT_PATH)/main.cpp
SERVER_MK = $(SDK_PATH)/server/serverless.mk

WEB_FILES += $(SDK_PATH)/web/index.html

OVERRIDE_DTSI := $(PROJECT_PATH)/override.dtsi