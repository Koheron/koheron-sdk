# http://clarkgrubb.com/makefile-style-guide
SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.SUFFIXES:

CONFIG ?= examples/led-blinker/config.yml
SDK_PATH ?= .
MODE ?= development
HOST ?= 192.168.1.100
TMP ?= tmp

KOHERON_VERSION := 0.16.0
VIVADO_VERSION := 2017.2

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
TMP_NAME := $(shell basename $(PROJECT_PATH))
TMP_PROJECT_PATH := $(TMP)/$(TMP_NAME)

# Python script that manages the instrument configuration
MAKE_PY := SDK_PATH=$(SDK_PATH) python $(SDK_PATH)/make.py

MEMORY_YML := $(TMP_PROJECT_PATH)/memory.yml

# Number of CPU cores available for parallel execution
N_CPUS := $(shell nproc 2> /dev/null || echo 1)

NAME := $(shell $(MAKE_PY) --name $(CONFIG) $(TMP_PROJECT_PATH)/name && cat $(TMP_PROJECT_PATH)/name)

###############################################################################
# INSTRUMENT
###############################################################################

# The instrument is packaged in a zip file that contains:
# - FPGA bitstream
# - TCP / Websocket server
# - Bash configuration script
# - Web files (HTML, CSS, Javascript)

BITSTREAM := $(TMP_PROJECT_PATH)/$(NAME).bit # FPGA bitstream
SERVER := $(TMP_PROJECT_PATH)/serverd # TCP / Websocket server executable that communicates with the FPGA:
START_SH := $(TMP_PROJECT_PATH)/start.sh # Bash script that configures the Zynq registers (clocks...)

# Zip file that contains all the files needed to run the instrument:
INSTRUMENT_ZIP := $(TMP_PROJECT_PATH)/$(NAME).zip
$(INSTRUMENT_ZIP): server $(BITSTREAM) $(START_SH) web
	zip --junk-paths $(INSTRUMENT_ZIP) $(BITSTREAM) $(SERVER) $(START_SH) $(WEB_ASSETS)
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
# WEB FILES
###############################################################################
WEB_PATH := $(SDK_PATH)/web
WEB_MK ?= $(WEB_PATH)/web.mk
include $(WEB_MK)

###############################################################################
# LINUX OS
###############################################################################
OS_PATH := $(SDK_PATH)/os
OS_MK ?= $(OS_PATH)/os.mk
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
	apt-get install -y g++-arm-linux-gnueabihf
	apt-get install -y python-pip
	apt-get install -y curl
	pip install -r $(SDK_PATH)/requirements.txt
	pip install $(SDK_PATH)/python

.PHONY: setup_fpga
setup_fpga: setup_base
	rm -f /usr/bin/gmake && ln -s make /usr/bin/gmake

.PHONY: setup_server
setup_server: setup_base

.PHONY: setup_web
setup_web: setup_base
	apt-get install -y nodejs
	rm -f /usr/bin/node && ln -s /usr/bin/nodejs /usr/bin/node
	npm install typescript
	npm install @types/jquery@2.0.46 @types/jquery-mousewheel@3.1.5 websocket @types/node

.PHONY: setup_os
setup_os: setup_base
	apt-get install -y libssl-dev bc device-tree-compiler qemu-user-static zerofree
	apt-get install -y lib32stdc++6 lib32z1 u-boot-tools

###############################################################################
# CLEAN TARGETS
###############################################################################

# The "clean" target only removes the files related to the instrument specified by $(NAME)
# Use "clean_all" to remove everything
.PHONY: clean
clean:
	rm -rf $(patsubst %/.,,$(TMP_PROJECT_PATH))

.PHONY: clean_all
clean_all:
	rm -rf $(TMP)

