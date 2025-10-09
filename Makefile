# http://clarkgrubb.com/makefile-style-guide
SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := all
# Cleaner logs when running -j
MAKEFLAGS += --output-sync=target
#MAKEFLAGS += --shuffle

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
VENV := .venv
HSI := source $(VIVADO_PATH)/settings64.sh && xsct
BOOTGEN := source $(VIVADO_PATH)/settings64.sh && bootgen
GCC_VERSION := 13

DOCKER_PATH := $(SDK_PATH)/docker
OS_PATH := $(SDK_PATH)/os
FPGA_PATH := $(SDK_PATH)/fpga
SERVER_PATH := $(SDK_PATH)/server
WEB_PATH := $(SDK_PATH)/web

ifndef CFG
$(error CFG is not defined. Please set CFG to the path of a config.mk file, e.g. `make CFG=examples/<board>/<instrument>/config.mk`.)
endif

CONFIG_MK := $(CFG)

ifeq ("$(wildcard $(CONFIG_MK))","")
$(error CFG '$(CFG)' does not reference an existing config.mk file.)
endif

BOARD_MK ?= $(BOARD_PATH)/board.mk
DOCKER_MK ?= $(DOCKER_PATH)/docker.mk
FPGA_MK ?= $(FPGA_PATH)/fpga.mk
OS_MK ?= $(OS_PATH)/os.mk
SERVER_MK ?= $(SERVER_PATH)/server.mk
WEB_MK ?= $(WEB_PATH)/web.mk

PROJECT_PATH := $(patsubst %/,%,$(dir $(CONFIG_MK)))
TMP_PROJECT_PATH := $(TMP)/$(PROJECT_PATH)

XDC :=
CORES :=
DRIVERS :=
WEB_FILES := $(SDK_PATH)/web/main.css $(SDK_PATH)/web/koheron.ts

.PHONY: help
help:
	@echo ' - all          : (Default goal) build the instrument: fpga, server and web'
	@echo ' - run          : Run the instrument'
	@echo ' - fpga         : Build the FPGA bitstream'
	@echo ' - server       : Build the server'
	@echo ' - web          : Build the web interface'
	@echo ' - os           : Build the operating system'
	@echo ' - image        : Build the full image'
	@echo ' - block_design : Build the Vivado block design interactively'
	@echo ' - open_project : Open the Vivado .xpr project'

# Python script that manages the instrument configuration
MAKE_PY := SDK_PATH=$(SDK_PATH) $(VENV)/bin/python3 $(SDK_PATH)/make.py

# Number of CPU cores available for parallel execution
N_CPUS ?= $(shell nproc 2> /dev/null || echo 1)

# TCP / Websocket server executable that communicates with the FPGA
SERVER := $(TMP_PROJECT_PATH)/serverd

VERSION_FILE := $(TMP_PROJECT_PATH)/version

include $(CONFIG_MK)

MEMORY_YML ?= $(PROJECT_PATH)/memory.yml
BD_TCL ?= $(PROJECT_PATH)/block_design.tcl
TCL_FILES ?= $(BD_TCL) $(wildcard $(PROJECT_PATH)/tcl/*.tcl)

ifdef VERBOSE
$(info ------------------------)
$(info CONFIG_MK = $(CONFIG_MK))
$(info ------------------------)
$(info VERSION   = $(VERSION))
$(info NAME      = $(NAME))
$(info BD_TCL    = $(BD_TCL))
$(info TCL_FILES = $(TCL_FILES))
$(info XDC       = $(XDC))
$(info CORES     = $(CORES))
$(info DRIVERS   = $(DRIVERS))
$(info WEB_FILES = $(WEB_FILES))
$(info )
endif

include $(BOARD_MK)
ifdef VERBOSE
$(info ------------------------)
$(info BOARD_MK = $(BOARD_MK))
$(info ------------------------)
$(info BOARD             = $(BOARD))
$(info PART              = $(PART))
$(info ZYNQ_TYPE         = $(ZYNQ_TYPE))
$(info TMP_OS_BOARD_PATH = $(TMP_OS_BOARD_PATH))
$(info FSBL_PATH         = $(FSBL_PATH))
$(info PATCHES           = $(PATCHES))
$(info )
endif

BITSTREAM := $(TMP_PROJECT_PATH)/$(NAME).bit

$(VERSION_FILE): | $(TMP_PROJECT_PATH)/
	@printf '%s\n' '$(VERSION)' > $@

include $(OS_PATH)/$(ZYNQ_TYPE).mk
include $(OS_PATH)/linux.mk
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
# - Web files (HTML, CSS, Javascript)

# Zip file that contains all the files needed to run the instrument:
INSTRUMENT_ZIP := $(TMP_PROJECT_PATH)/$(NAME).zip
$(INSTRUMENT_ZIP): $(SERVER) $(BITSTREAM) $(WEB_ASSETS) $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM).bin $(VERSION_FILE) | $(TMP_PROJECT_PATH)/ $(TMP)/$(BOARD)/instruments/
	zip --junk-paths $(INSTRUMENT_ZIP) $(BITSTREAM).bin $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM) $(SERVER) $(WEB_ASSETS) $(VERSION_FILE)
	cp $(INSTRUMENT_ZIP) $(TMP)/$(BOARD)/instruments/$(NAME).zip
	$(call ok,$@)

# Make builds the instrument zip file by default
.PHONY: all
all: $(INSTRUMENT_ZIP)

# The "run" target launches the instrument on the Zynq board
# this is done via the HTTP API (see os/api)
.PHONY: run
run: ccache-prepare $(INSTRUMENT_ZIP)
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

.PHONY: setup
setup:
	sudo apt-get install -y curl rsync python3-venv
	[ -d $(VENV) ] || python3 -m venv $(VENV)
	$(VENV)/bin/python3 -m ensurepip --upgrade
	$(VENV)/bin/python3 -m pip install --upgrade pip
	$(PIP) install -r $(SDK_PATH)/requirements.txt
	$(PIP) install $(SDK_PATH)/python
	bash docker/install_docker.sh
	sudo usermod -aG docker $(shell whoami)
	docker build -f $(DOCKER_PATH)/Dockerfile -t $(DOCKER_IMAGE) $(DOCKER_PATH)
	docker build -f $(WEB_PATH)/Dockerfile.web -t $(WEB_DOCKER_IMAGE) $(WEB_PATH)

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