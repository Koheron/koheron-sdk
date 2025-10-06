NAME := bram
VERSION := 0.0.0

BOARD_PATH := $(SDK_PATH)/boards/red-pitaya

MEMORY_YML = $(PROJECT_PATH)/memory.yml

XDC += $(PROJECT_PATH)/constraints.xdc

SERVER_MK = $(PROJECT_PATH)/app.mk

WEB_FILES += $(SDK_PATH)/web/index.html