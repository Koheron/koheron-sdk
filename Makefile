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
include $(SDK_PATH)/make/defaults.mk
include $(SDK_PATH)/make/dependencies.mk

.PHONY: FORCE
FORCE:

include $(SDK_PATH)/make/config.mk

# Compose the build only when the requested commands need an instrument.
ifneq ($(strip $(CFG_REQUIRED_GOALS)),)

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

include $(SDK_PATH)/make/instrument.mk

# Make builds the instrument zip file by default
.PHONY: all
all: $(INSTRUMENT_ZIP) $(BOARD_INSTRUMENT_ZIP)

# The "run" target launches the instrument on the Zynq board
# this is done via the HTTP API (see os/api)
.PHONY: run
run: $(INSTRUMENT_ZIP) $(KOHERON_PYTHON_STAMP)
	$(VENV)/bin/python3 -m koheron.instrument_runner --host $(HOST) --name $(NAME) $(INSTRUMENT_ZIP)
	@echo

.PHONY: test
test: $(KOHERON_PYTHON_STAMP)
	@test -n "$(INSTRUMENT_TEST)" || { echo 'No hardware test script found; use make check for local checks.' >&2; exit 1; }
	HOST=$(HOST) $(VENV)/bin/python3 $(call build_shell_quote,$(INSTRUMENT_TEST))

INSTRUMENT_TEST ?= $(firstword $(wildcard $(PROJECT_PATH)/test.py $(PROJECT_PATH)/python/test.py))

.PHONY: deploy
deploy:
	+@$(MAKE) --no-print-directory $(call build_shell_quote,$(INSTRUMENT_ZIP)) >&2
	@$(SDK_MANAGEMENT_COMMAND) deploy $(call build_shell_quote,$(INSTRUMENT_ZIP)) --name $(call build_shell_quote,$(NAME))

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
	$(HOST_PYTHON) $(SDK_PATH)/python/koheron/safe_clean.py --sdk-path $(call build_shell_quote,$(SDK_FULL_PATH)) --root $(call build_shell_quote,$(TMP)) --target $(call build_shell_quote,$(TMP_PROJECT_PATH))

else

DOCKER_IMAGE ?= cross-armhf:$(DOCKER_UBUNTU_VERSION)
WEB_DOCKER_IMAGE ?= koheron-web:node24

endif

include $(SDK_PATH)/make/host.mk
include $(SDK_PATH)/make/setup.mk
