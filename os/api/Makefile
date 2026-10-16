CXX = g++-15
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
.DEFAULT_GOAL := all
BUILD_DIR ?= build
PKG_CONFIG ?= pkg-config
BUILD_RULES := $(abspath $(lastword $(MAKEFILE_LIST)))
SOURCE_DIR := $(dir $(BUILD_RULES))native
include $(SOURCE_DIR)/../../../make/dependencies.mk
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags libmicrohttpd libzip json-c libsystemd)
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++23 -Wall -Wextra -Wpedantic -Werror -pthread -MMD -MP
LDLIBS += $(shell $(PKG_CONFIG) --libs libmicrohttpd libzip json-c libsystemd) -lunistring -pthread
COMMON := $(BUILD_DIR)/common.o
ARCHIVE := $(BUILD_DIR)/archive.o
BINARIES := $(addprefix $(BUILD_DIR)/,koheron-api koheron-install koheron-server-init)
API_COMPILE_SETTINGS := $(BUILD_DIR)/.compile-settings
API_LINK_SETTINGS := $(BUILD_DIR)/.link-settings
$(eval $(call build_settings_stamp,$(API_COMPILE_SETTINGS),$$(call build_shell_quote,$$(CXX)) $$(call build_shell_quote,$$(CPPFLAGS)) $$(call build_shell_quote,$$(CXXFLAGS))))
$(eval $(call build_settings_stamp,$(API_LINK_SETTINGS),$$(call build_shell_quote,$$(CXX)) $$(call build_shell_quote,$$(CXXFLAGS)) $$(call build_shell_quote,$$(LDFLAGS)) $$(call build_shell_quote,$$(LDLIBS))))

.PHONY: all clean journal-probe
all: $(BINARIES)

# Host test helper only; it is not included in board binaries or the OS image.
journal-probe: $(BUILD_DIR)/journal-probe
$(BUILD_DIR)/journal-probe: $(SOURCE_DIR)/../tests/journal_probe.cpp $(BUILD_DIR)/journal.o $(COMMON) $(BUILD_RULES) $(COMPILER_SETTINGS) $(API_COMPILE_SETTINGS) $(API_LINK_SETTINGS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp %.o,$^) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/koheron-api: $(BUILD_DIR)/api_main.o $(BUILD_DIR)/journal.o $(BUILD_DIR)/health.o $(BUILD_DIR)/telemetry.o $(BUILD_DIR)/events.o $(BUILD_DIR)/sha1.o $(COMMON) $(ARCHIVE)

SDK_ROOT := $(abspath $(SOURCE_DIR)/../../..)
CPPFLAGS += -I$(SDK_ROOT)
$(BUILD_DIR)/sha1.o: $(SDK_ROOT)/server/network/sha1.cpp $(SDK_ROOT)/server/network/sha1.hpp $(BUILD_RULES) $(COMPILER_SETTINGS) $(API_COMPILE_SETTINGS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/koheron-install: $(BUILD_DIR)/install_main.o $(COMMON) $(ARCHIVE)

$(BUILD_DIR)/koheron-server-init: $(BUILD_DIR)/led_main.o $(COMMON)

$(BINARIES): $(API_LINK_SETTINGS)
	$(CXX) $(CXXFLAGS) $(filter %.o,$^) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp $(BUILD_RULES) $(COMPILER_SETTINGS) $(API_COMPILE_SETTINGS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(wildcard $(BUILD_DIR)/*.d)
clean:
	rm -rf $(BUILD_DIR)
