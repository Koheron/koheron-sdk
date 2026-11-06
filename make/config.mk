# Decide whether this invocation needs an instrument, then resolve its inputs.
CFG_OPTIONAL_GOALS := help doctor check status logs list examples clean_all setup python_requirements koheron_python $(PYTHON_REQUIREMENTS_STAMP) $(KOHERON_PYTHON_STAMP)

ifneq ($(MAKECMDGOALS),)
# copy reads the source configuration without including the build graph.
CFG_REQUIRED_GOALS := $(filter-out $(CFG_OPTIONAL_GOALS) copy,$(MAKECMDGOALS))
else
CFG_REQUIRED_GOALS := all
endif
ifneq ($(strip $(CFG)),)
CFG_REQUIRED_GOALS += $(filter doctor check,$(MAKECMDGOALS))
endif

ifneq ($(filter validate,$(MAKECMDGOALS)),)
ifndef CFG
$(call fail,CFG is not defined. Please set CFG to the path of a config.mk file, e.g. `make validate CFG=examples/<board>/<instrument>/config.mk`.)
endif
endif

ifneq ($(filter copy,$(MAKECMDGOALS)),)
ifndef CFG
$(call fail,Use `make copy CFG=examples/<board>/<instrument>/config.mk DEST=examples/<board>/<new-name>`.)
endif
ifndef DEST
$(call fail,DEST is not defined. Set DEST to the new instrument directory.)
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
# Resolve CFG before placing artifacts: relative paths containing '..' must not
# escape TMP, and external instruments need a stable cache location.
PROJECT_CANONICAL_PATH := $(realpath $(PROJECT_PATH))
ifeq ($(PROJECT_CANONICAL_PATH),$(SDK_FULL_PATH))
PROJECT_CACHE_PATH := .external/$(shell printf '%s' $(call build_shell_quote,$(PROJECT_CANONICAL_PATH)) | sha256sum | cut -d' ' -f1)
else ifneq ($(filter $(SDK_FULL_PATH)/%,$(PROJECT_CANONICAL_PATH)),)
PROJECT_CACHE_PATH := $(patsubst $(SDK_FULL_PATH)/%,%,$(PROJECT_CANONICAL_PATH))
else
PROJECT_CACHE_PATH := .external/$(shell printf '%s' $(call build_shell_quote,$(PROJECT_CANONICAL_PATH)) | sha256sum | cut -d' ' -f1)
endif
ifneq ($(notdir $(CONFIG_MK)),config.mk)
# Board selectors share sources but must keep generated hardware/software apart.
PROJECT_CACHE_PATH := $(PROJECT_CACHE_PATH)/$(basename $(notdir $(CONFIG_MK)))
endif
TMP_PROJECT_PATH := $(TMP)/$(PROJECT_CACHE_PATH)
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
# Shared instrument fragments participate in FPGA project rebuilds.
CONFIG_MK_DEPS := $(MAKEFILE_LIST)

MEMORY_YML ?= $(PROJECT_PATH)/memory.yml
BD_TCL ?= $(PROJECT_PATH)/block_design.tcl
include $(FPGA_PATH)/lib/dependencies.mk
# Custom instruments retain the conservative shared-library dependency set.
FPGA_LIB_TCL ?= $(wildcard $(FPGA_PATH)/lib/*.tcl)
TCL_FILES ?= $(BD_TCL) $(wildcard $(PROJECT_PATH)/*.tcl $(PROJECT_PATH)/tcl/*.tcl) $(wildcard $(BOARD_PATH)/*.tcl $(BOARD_PATH)/config/*.tcl) $(FPGA_LIB_TCL) $(TCL_EXTRA_FILES)

INSTRUMENT_ZIP := $(TMP_PROJECT_PATH)/$(NAME).zip

ifneq ($(if $(filter-out json,$(FORMAT)),$(VERBOSE)),)
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
ifneq ($(if $(filter-out json,$(FORMAT)),$(VERBOSE)),)
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

endif
