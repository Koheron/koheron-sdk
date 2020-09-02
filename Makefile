# http://clarkgrubb.com/makefile-style-guide
SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.SUFFIXES:

CONFIG ?= examples/red-pitaya/led-blinker/config.yml
SDK_PATH ?= .
MODE ?= development
HOST ?= 192.168.1.100
TMP ?= tmp

KOHERON_VERSION_FILE := $(SDK_PATH)/version
KOHERON_VERSION := $(shell cat $(KOHERON_VERSION_FILE))
VIVADO_VERSION := 2020.1
VIVADO_PATH := /tools/Xilinx/Vivado
VITIS_PATH := /tools/Xilinx/Vitis
PYTHON := python
PIP:= pip

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

# Directory for storing the build artifacts
PROJECT_PATH := $(dir $(CONFIG))
TMP_PROJECT_PATH := $(TMP)/$(PROJECT_PATH)

# Python script that manages the instrument configuration
MAKE_PY := SDK_PATH=$(SDK_PATH) $(PYTHON) $(SDK_PATH)/make.py

MEMORY_YML := $(TMP_PROJECT_PATH)/memory.yml

# Number of CPU cores available for parallel execution
N_CPUS ?= $(shell nproc 2> /dev/null || echo 1)

NAME := $(shell $(MAKE_PY) --name $(CONFIG) $(TMP_PROJECT_PATH)/name && cat $(TMP_PROJECT_PATH)/name)

###############################################################################
# INSTRUMENT
###############################################################################

# The instrument is packaged in a zip file that contains:
# - FPGA bitstream
# - TCP / Websocket server
# - Bash configuration script
# - Web files (HTML, CSS, Javascript)

BITSTREAM := $(TMP_PROJECT_PATH)/$(NAME).bit
# FPGA bitstream
SERVER := $(TMP_PROJECT_PATH)/serverd # TCP / Websocket server executable that communicates with the FPGA

VERSION_FILE := $(TMP_PROJECT_PATH)/version

$(VERSION_FILE): $(CONFIG)
	$(MAKE_PY) --version $(CONFIG) $@

# Zip file that contains all the files needed to run the instrument:
INSTRUMENT_ZIP := $(TMP_PROJECT_PATH)/$(NAME).zip
$(INSTRUMENT_ZIP): server $(BITSTREAM) web $(VERSION_FILE) $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM).bin
	zip --junk-paths $(INSTRUMENT_ZIP) $(BITSTREAM).bin $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM) $(SERVER) $(WEB_ASSETS) $(VERSION_FILE)
	@echo [$@] OK

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
# FPGA BITSTREAM
###############################################################################
OS_PATH := $(SDK_PATH)/os
OS_MK ?= $(OS_PATH)/os.mk
FPGA_PATH := $(SDK_PATH)/fpga
FPGA_MK ?= $(FPGA_PATH)/fpga.mk
include $(FPGA_MK)

###############################################################################
# TCP / WEBSOCKET SERVER
###############################################################################
SERVER_PATH := $(SDK_PATH)/server
SERVER_MK ?= $(SERVER_PATH)/server.mk
include $(SERVER_MK)

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
# WEB FILES
###############################################################################
WEB_PATH := $(SDK_PATH)/web
WEB_MK ?= $(WEB_PATH)/web.mk
include $(WEB_MK)

###############################################################################
# LINUX OS
###############################################################################
include $(OS_MK)

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
setup: setup_fpga setup_server setup_web setup_os

.PHONY: setup_base
setup_base:
	sudo apt-get install -y g++-5-arm-linux-gnueabihf
	sudo apt install -y g++-aarch64-linux-gnu
	# On Ubuntu 18.04 you may have to link:	
	# sudo ln -s /usr/bin/arm-linux-gnueabihf-gcc-5 /usr/bin/arm-linux-gnueabihf-gcc
	# sudo ln -s /usr/bin/arm-linux-gnueabihf-g++-5 /usr/bin/arm-linux-gnueabihf-g++	
	sudo apt-get install -y python-$(PIP)
	sudo apt-get install -y curl
	$(PIP) install -r $(SDK_PATH)/requirements.txt
	$(PIP) install $(SDK_PATH)/python

.PHONY: setup_fpga
setup_fpga: setup_base
	sudo apt-get install device-tree-compiler
	sudo rm -f /usr/bin/gmake && sudo ln -s make /usr/bin/gmake

.PHONY: setup_server
setup_server: setup_base

.PHONY: setup_web
setup_web: setup_base
	sudo apt-get install -y nodejs
	sudo apt-get install -y node-typescript
	sudo apt-get install -y npm # npm installed with nodejs
	#sudo rm -f /usr/bin/node && sudo ln -s /usr/bin/nodejs /usr/bin/node
	npm install typescript
	npm install @types/jquery@2.0.46 @types/jquery-mousewheel@3.1.5 websocket @types/node

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