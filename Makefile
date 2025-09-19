# http://clarkgrubb.com/makefile-style-guide
SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := all
# Cleaner logs when running -j
MAKEFLAGS += --output-sync=target

.DELETE_ON_ERROR:
.SUFFIXES:
PYTHONPATH :=
export PYTHONPATH
MATLABPATH :=
export MATLABPATH

GREEN := \033[1;32m
RESET := \033[0m

ok = @printf '%b\n' '$(GREEN)[$(1)] OK$(RESET)'

%/:
	mkdir -p $@

SDK_PATH ?= .
MODE ?= development
SDK_FULL_PATH = $(realpath $(SDK_PATH))
HOST ?= 192.168.1.100
TMP ?= tmp

KOHERON_VERSION := 1.0
VIVADO_VERSION := 2025.1
VIVADO_PATH := /tools/Xilinx/$(VIVADO_VERSION)/Vivado
VITIS_PATH := /tools/Xilinx/$(VIVADO_VERSION)/Vitis
PYTHON := python3
VENV := .venv
HSI := source $(VIVADO_PATH)/settings64.sh && xsct
BOOTGEN := source $(VIVADO_PATH)/settings64.sh && bootgen
GCC_VERSION := 13

DOCKER_PATH := $(SDK_PATH)/docker
OS_PATH := $(SDK_PATH)/os
FPGA_PATH := $(SDK_PATH)/fpga
SERVER_PATH := $(SDK_PATH)/server
WEB_PATH := $(SDK_PATH)/web

# Use config.mk instead of config.yml
CONFIG_MK ?= examples/alpha250/fft/config.mk
BOARD_MK ?= $(BOARD_PATH)/board.mk
DOCKER_MK ?= $(DOCKER_PATH)/docker.mk
FPGA_MK ?= $(FPGA_PATH)/fpga.mk
OS_MK ?= $(OS_PATH)/os.mk
SERVER_MK ?= $(SERVER_PATH)/server.mk
WEB_MK ?= $(WEB_PATH)/web.mk

PROJECT_PATH := $(dir $(CONFIG_MK))
TMP_PROJECT_PATH := $(TMP)/$(PROJECT_PATH)

CORES :=

#BUILD_METHOD := native
BUILD_METHOD = docker

.PHONY: help
help:
	@echo ' - all          : (Default goal) build the instrument: fpga, server and web'
	@echo ' - run          : Run the instrument'
	@echo ' - fpga         : Build the FPGA bitstream'
	@echo ' - server       : Build the server'
	@echo ' - web          : Build the web interface'
	@echo ' - os           : Build the operating system'
	@echo ' - image        : Build the root file system (run as root)'
	@echo ' - block_design : Build the Vivado block design interactively'
	@echo ' - open_project : Open the Vivado .xpr project'

# Python script that manages the instrument configuration
MAKE_PY := SDK_PATH=$(SDK_PATH) $(VENV)/bin/$(PYTHON) $(SDK_PATH)/make.py

MEMORY_YML := $(TMP_PROJECT_PATH)/memory.yml

# Number of CPU cores available for parallel execution
N_CPUS ?= $(shell nproc 2> /dev/null || echo 1)

# TCP / Websocket server executable that communicates with the FPGA
SERVER := $(TMP_PROJECT_PATH)/serverd

VERSION_FILE := $(TMP_PROJECT_PATH)/version

include $(CONFIG_MK)
include $(BOARD_MK)

BITSTREAM := $(TMP_PROJECT_PATH)/$(NAME).bit

$(VERSION_FILE): | $(TMP_PROJECT_PATH)/
	@printf '%s\n' '$(VERSION)' > $@

include $(OS_PATH)/$(ZYNQ_TYPE).mk
include $(DOCKER_MK)
include $(FPGA_MK)
include $(OS_MK)
include $(SERVER_MK)
include $(WEB_MK)

###############################################################################
# INSTRUMENT
###############################################################################

# The instrument is packaged in a zip file that contains:
# - FPGA bitstream
# - TCP / Websocket server
# - Bash configuration script
# - Web files (HTML, CSS, Javascript)

# Zip file that contains all the files needed to run the instrument:
INSTRUMENT_ZIP := $(TMP_PROJECT_PATH)/$(NAME).zip
$(INSTRUMENT_ZIP): $(SERVER) $(BITSTREAM) $(WEB_ASSETS) $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM).bin $(VERSION_FILE)
	zip --junk-paths $(INSTRUMENT_ZIP) $(BITSTREAM).bin $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM) $(SERVER) $(WEB_ASSETS) $(VERSION_FILE)
	$(call ok,$@)

# Make builds the instrument zip file by default
.PHONY: all
all: $(INSTRUMENT_ZIP)

# The "run" target launches the instrument on the Zynq board
# this is done via the HTTP API (see os/api)
.PHONY: run
run: $(INSTRUMENT_ZIP)
	curl -v -F $(NAME).zip=@$(INSTRUMENT_ZIP) http://$(HOST)/api/instruments/upload
	curl http://$(HOST)/api/instruments/run/$(NAME)
	@echo

###############################################################################
# C++ CLIENT
###############################################################################
CLIENT_PATH := $(PROJECT_PATH)/client
ifneq ("$(wildcard $(CLIENT_PATH)/client.mk)","")
-include $(CLIENT_PATH)/client.mk
else
PHONY: client
client:
	@echo 'No client available for this instrument'
endif

###############################################################################
# TESTS
###############################################################################
TESTS_PATH := $(SDK_PATH)/tests
TESTS_MK ?= $(TESTS_PATH)/tests.mk
include $(TESTS_MK)

###############################################################################
# PYTHON
###############################################################################
PYTHON_PATH := $(SDK_PATH)/python
PYTHON_MK ?= $(PYTHON_PATH)/python.mk
include $(PYTHON_MK)

###############################################################################
# SETUP TARGETS
###############################################################################

.PHONY: setup
setup: setup_docker setup_fpga setup_server setup_web setup_os

.PHONY: setup_base
setup_base:
# 	sudo apt-get install -y g++-$(GCC_VERSION)-arm-linux-gnueabihf
# 	sudo rm -f /usr/bin/arm-linux-gnueabihf-gcc /usr/bin/arm-linux-gnueabihf-g++
# 	sudo ln -s /usr/bin/arm-linux-gnueabihf-gcc-$(GCC_VERSION) /usr/bin/arm-linux-gnueabihf-gcc
# 	sudo ln -s /usr/bin/arm-linux-gnueabihf-g++-$(GCC_VERSION) /usr/bin/arm-linux-gnueabihf-g++
	sudo apt-get install -y curl rsync $(PYTHON)-venv
	[ -d $(VENV) ] || $(PYTHON) -m venv $(VENV)
	$(VENV)/bin/$(PYTHON) -m ensurepip --upgrade
	$(VENV)/bin/$(PYTHON) -m pip install --upgrade pip
	$(PIP) install -r $(SDK_PATH)/requirements.txt
	$(PIP) install $(SDK_PATH)/python

.PHONY: setup_docker
setup_docker: setup_base
	sudo bash docker/install_docker.sh
	sudo usermod -aG docker $(shell whoami)
	sudo docker build -t cross-armhf:24.04 ./docker/.

.PHONY: setup_fpga
setup_fpga: setup_base
	sudo apt-get install device-tree-compiler
	sudo rm -f /usr/bin/gmake && sudo ln -s make /usr/bin/gmake

.PHONY: setup_server
setup_server: setup_base

.PHONY: setup_web
setup_web: setup_base web_builder

.PHONY: setup_os
setup_os: setup_base
	sudo apt-get install -y libssl-dev bc qemu-user-static zerofree
	sudo apt-get install -y lib32stdc++6 lib32z1 u-boot-tools

###############################################################################
# CLEAN TARGETS
###############################################################################

# The "clean" target only removes the files related to the instrument specified by $(NAME)
# Use "clean_all" to remove everything
.PHONY: clean
clean:
	rm -rf $(patsubst %/.,%,$(TMP_PROJECT_PATH))

.PHONY: clean_all
clean_all:
	rm -rf $(TMP)
