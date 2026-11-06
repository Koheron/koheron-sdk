# Host, toolchain and path defaults. Load before the selected instrument.
MODE ?= development
SDK_FULL_PATH = $(realpath $(SDK_PATH))
# Ensure every recipe prints a colorized error message when a shell command fails.
MAKE_BASH_ENV := $(SDK_FULL_PATH)/.make-bash-env
export BASH_ENV := $(MAKE_BASH_ENV)
HOST ?= 192.168.1.100
TMP ?= tmp
FORMAT ?= text
TARGET ?= all
TIMEOUT ?= $(if $(filter deploy,$(MAKECMDGOALS)),120,10)
LOG_LIMIT ?= 100

PYTHON_VERSION := 3
KOHERON_VERSION := 1.0
VIVADO_VERSION := 2025.1
VIVADO_PATH := /tools/Xilinx/$(VIVADO_VERSION)/Vivado
VITIS_PATH := /tools/Xilinx/$(VIVADO_VERSION)/Vitis
VENV := .venv
PIP ?= $(VENV)/bin/pip
PYTHON_REQUIREMENTS_STAMP := $(VENV)/.requirements.stamp
KOHERON_PYTHON_STAMP := $(VENV)/.koheron-python.stamp
VIVADO_MAJOR_VER = $(shell echo $(VIVADO_VERSION) | cut -d. -f1)
ifeq ($(shell test $(VIVADO_MAJOR_VER) -ge 2024 && echo "true"),true)
    # Vitis 2024+ - Use xsdb (XSCT deprecated)
    HSI := source $(VIVADO_PATH)/settings64.sh && xsdb
else
    # Legacy versions (2023.2 and earlier) - Use classic xsct
    HSI := source $(VIVADO_PATH)/settings64.sh && xsct
endif
BOOTGEN := source $(VIVADO_PATH)/settings64.sh && bootgen
GCC_VERSION := 15
DOCKER_UBUNTU_VERSION ?= 26.04
DOCKER_IMAGE ?= cross-armhf:$(DOCKER_UBUNTU_VERSION)

DOCKER_PATH := $(SDK_PATH)/docker
OS_PATH := $(SDK_PATH)/os
FPGA_PATH := $(SDK_PATH)/fpga
SERVER_PATH := $(SDK_PATH)/server
WEB_PATH := $(SDK_PATH)/web
HOST_PYTHON ?= $(if $(wildcard $(VENV)/bin/python3),$(VENV)/bin/python3,python3)
