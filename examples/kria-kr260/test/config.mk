NAME := test
VERSION := 0.1.0

BOARD_PATH := $(SDK_PATH)/boards/kria-kr260

MEMORY_YML = $(PROJECT_PATH)/memory.yml

SERVERLESS_CPP_SRCS := $(PROJECT_PATH)/main.cpp
SERVER_MK = $(SDK_PATH)/server/serverless.mk