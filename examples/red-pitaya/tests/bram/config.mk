NAME := bram
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(PROJECT_PATH)/constraints.xdc

PROJECT_CPP := $(PROJECT_PATH)/main.cpp
SERVER_MK = $(SDK_PATH)/server/serverless.mk

WEB_FILES += $(SDK_PATH)/web/index.html