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
RED := \033[1;31m
RESET := \033[0m

fail = $(error $(RED)[ERROR] $(1)$(RESET))

ok = @printf '%b\n' '$(GREEN)[$(1)] OK$(RESET)'

%/:
	mkdir -p $@

SDK_PATH ?= .
MODE ?= development
SDK_FULL_PATH = $(realpath $(SDK_PATH))
# Ensure every recipe prints a colorized error message when a shell command fails.
MAKE_BASH_ENV := $(SDK_FULL_PATH)/.make-bash-env
export BASH_ENV := $(MAKE_BASH_ENV)
HOST ?= 192.168.1.100
TMP ?= tmp

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
GCC_VERSION := 13

DOCKER_PATH := $(SDK_PATH)/docker
OS_PATH := $(SDK_PATH)/os
FPGA_PATH := $(SDK_PATH)/fpga
SERVER_PATH := $(SDK_PATH)/server
WEB_PATH := $(SDK_PATH)/web

CFG_OPTIONAL_GOALS := help doctor list examples setup python_requirements koheron_python $(PYTHON_REQUIREMENTS_STAMP) $(KOHERON_PYTHON_STAMP)

ifneq ($(MAKECMDGOALS),)
# validate parses CFG directly without including the full build graph.
CFG_REQUIRED_GOALS := $(filter-out $(CFG_OPTIONAL_GOALS) validate,$(MAKECMDGOALS))
else
CFG_REQUIRED_GOALS := all
endif

.PHONY: FORCE
FORCE:

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
	@echo ' - doctor       : Check host tools and optional CFG before building'
	@echo ' - validate     : Validate selected CFG and memory.yml'
	@echo ' - list         : List available example instruments'
	@echo ' - examples     : Alias for list'

ifneq ($(filter validate,$(MAKECMDGOALS)),)
ifndef CFG
$(call fail,CFG is not defined. Please set CFG to the path of a config.mk file, e.g. `make validate CFG=examples/<board>/<instrument>/config.mk`.)
endif
endif

ifneq ($(strip $(CFG_REQUIRED_GOALS)),)

ifndef CFG
$(call fail,CFG is not defined. Please set CFG to the path of a config.mk file, e.g. `make CFG=examples/<board>/<instrument>/config.mk`.)
endif

CONFIG_MK := $(CFG)

ifeq ("$(wildcard $(CONFIG_MK))","")
$(call fail,CFG '$(CFG)' does not reference an existing config.mk file.)
endif

BOARD_MK ?= $(BOARD_PATH)/board.mk
DOCKER_MK ?= $(DOCKER_PATH)/docker.mk
FPGA_MK ?= $(FPGA_PATH)/fpga.mk
OS_MK ?= $(OS_PATH)/os.mk
SERVER_MK ?= $(SERVER_PATH)/server.mk
WEB_MK ?= $(WEB_PATH)/web.mk

PROJECT_PATH := $(patsubst %/,%,$(dir $(CONFIG_MK)))
TMP_PROJECT_PATH := $(TMP)/$(PROJECT_PATH)
TMP_OS_PATH := $(TMP_PROJECT_PATH)/os

XDC :=
CORES :=
DRIVERS :=
WEB_FILES := $(SDK_PATH)/web/main.css $(SDK_PATH)/web/koheron.ts

# Python script that manages the instrument configuration
MAKE_PY = SDK_PATH=$(SDK_PATH) ARCH=$(ARCH) $(VENV)/bin/python3 $(SDK_PATH)/make.py

# Number of CPU cores available for parallel execution
N_CPUS ?= $(shell nproc 2> /dev/null || echo 1)

# TCP / Websocket server executable that communicates with the FPGA
SERVER := $(TMP_PROJECT_PATH)/serverd

VERSION_FILE := $(TMP_PROJECT_PATH)/version

include $(CONFIG_MK)

MEMORY_YML ?= $(PROJECT_PATH)/memory.yml
BD_TCL ?= $(PROJECT_PATH)/block_design.tcl
TCL_FILES ?= $(BD_TCL) $(wildcard $(PROJECT_PATH)/tcl/*.tcl)

INSTRUMENT_ZIP := $(TMP_PROJECT_PATH)/$(NAME).zip

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

$(VERSION_FILE): $(CONFIG_MK) | $(TMP_PROJECT_PATH)/
	@printf '%s\n' '$(VERSION)' > $@.tmp
	@cmp -s $@.tmp $@ || mv -f $@.tmp $@
	@rm -f $@.tmp

include $(OS_PATH)/$(ZYNQ_TYPE).mk
include $(OS_PATH)/linux.mk
include $(DOCKER_MK)
include $(FPGA_MK)
include $(OS_MK)
include $(OS_PATH)/rootfs.mk
include $(SERVER_MK)
include $(WEB_MK)

###############################################################################
# INSTRUMENT
###############################################################################

# The instrument is packaged in a zip file that contains:
# - FPGA bitstream
# - Device tree overlay
# - TCP / Websocket server
# - Web files (HTML, CSS, Javascript)

# Zip file that contains all the files needed to run the instrument:
$(INSTRUMENT_ZIP): $(SERVER) $(DRIVERS_JSON_OUT) $(BITSTREAM) $(WEB_ASSETS) $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM).bin $(VERSION_FILE) | $(TMP_PROJECT_PATH)/ $(TMP)/$(BOARD)/instruments/
	rm -f $(INSTRUMENT_ZIP)
	zip --junk-paths $(INSTRUMENT_ZIP) $(BITSTREAM).bin $(TMP_PROJECT_PATH)/pl.dtbo $(BITSTREAM) $(SERVER) $(DRIVERS_JSON_OUT) $(WEB_ASSETS) $(VERSION_FILE)
	cp $(INSTRUMENT_ZIP) $(TMP)/$(BOARD)/instruments/$(NAME).zip
	$(call ok,$@)

# Make builds the instrument zip file by default
.PHONY: all
all: $(INSTRUMENT_ZIP)

# The "run" target launches the instrument on the Zynq board
# this is done via the HTTP API (see os/api)
.PHONY: run
run: $(INSTRUMENT_ZIP) $(KOHERON_PYTHON_STAMP)
	$(VENV)/bin/python3 -m koheron.instrument_runner --host $(HOST) --name $(NAME) $(INSTRUMENT_ZIP)
	@echo

.PHONY: test
test: $(KOHERON_PYTHON_STAMP)
	HOST=$(HOST) $(VENV)/bin/python3 $(PROJECT_PATH)/test.py

###############################################################################
# C++ CLIENT
###############################################################################
CLIENT_PATH := $(PROJECT_PATH)/client
ifneq ("$(wildcard $(CLIENT_PATH)/client.mk)","")
-include $(CLIENT_PATH)/client.mk
else
.PHONY: client
client:
	@echo 'No client available for this instrument'
endif

###############################################################################
# PYTHON
###############################################################################
PYTHON_PATH := $(SDK_PATH)/python
PYTHON_MK ?= $(PYTHON_PATH)/python.mk
include $(PYTHON_MK)

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

else

DOCKER_IMAGE ?= cross-armhf:24.04
WEB_DOCKER_IMAGE ?= koheron-web:node20

endif

.PHONY: list examples
list:
	@python3 "$(SDK_PATH)/python/koheron/list_examples.py" --sdk-path "$(SDK_PATH)"

examples: list

.PHONY: doctor
doctor:
	python3 "$(SDK_PATH)/python/koheron/doctor.py" \
		--sdk-path "$(SDK_PATH)" \
		--xilinx-version "$(VIVADO_VERSION)" \
		--vivado-path "$(VIVADO_PATH)" \
		--vitis-path "$(VITIS_PATH)" $(if $(CFG),--cfg "$(CFG)",)

.PHONY: validate
validate: $(PYTHON_REQUIREMENTS_STAMP)
	"$(VENV)/bin/python$(PYTHON_VERSION)" "$(SDK_PATH)/python/koheron/validate_config.py" \
		--sdk-path "$(SDK_PATH)" \
		--cfg "$(CFG)"

###############################################################################
# PYTHON SETUP
###############################################################################

PYTHON_PACKAGE_FILES := $(shell find $(SDK_PATH)/python -type f \
	\( -name '*.py' -o -name 'setup.py' -o -name 'pyproject.toml' -o -name 'setup.cfg' \) 2>/dev/null)

$(PYTHON_REQUIREMENTS_STAMP): $(SDK_PATH)/requirements.txt
	@mkdir -p $(@D)
	@[ -x $(VENV)/bin/python$(PYTHON_VERSION) ] || python$(PYTHON_VERSION) -m venv $(VENV)
	@$(VENV)/bin/python$(PYTHON_VERSION) -m ensurepip --upgrade >/dev/null
	@$(VENV)/bin/python$(PYTHON_VERSION) -m pip install --upgrade pip
	@$(VENV)/bin/python$(PYTHON_VERSION) -m pip install -r $(SDK_PATH)/requirements.txt
	@touch $@
	$(call ok,python requirements)

$(KOHERON_PYTHON_STAMP): $(PYTHON_REQUIREMENTS_STAMP) $(PYTHON_PACKAGE_FILES)
	@$(VENV)/bin/python$(PYTHON_VERSION) -m pip install $(SDK_PATH)/python
	@touch $@
	$(call ok,koheron python package)

.PHONY: python_requirements koheron_python
python_requirements: $(PYTHON_REQUIREMENTS_STAMP)
koheron_python: $(KOHERON_PYTHON_STAMP)

DISTRO := $(shell bash ./.setup/get_distro.sh)
.PHONY: setup
setup:
	sudo bash .setup/install_dependencies_$(DISTRO).sh
	$(MAKE) --no-print-directory $(KOHERON_PYTHON_STAMP)
	bash docker/install_docker.sh
	sudo usermod -aG docker $(shell whoami)
	docker build -f $(DOCKER_PATH)/Dockerfile -t $(DOCKER_IMAGE) $(DOCKER_PATH)
	docker build -f $(WEB_PATH)/Dockerfile.web -t $(WEB_DOCKER_IMAGE) $(WEB_PATH)
