CXX = g++-15
BUILD_DIR ?= build
PKG_CONFIG ?= pkg-config
BUILD_RULES := $(abspath $(lastword $(MAKEFILE_LIST)))
SOURCE_DIR := $(dir $(BUILD_RULES))native
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags libmicrohttpd libzip json-c libsystemd)
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++23 -Wall -Wextra -Wpedantic -Werror -pthread -MMD -MP
LDLIBS += $(shell $(PKG_CONFIG) --libs libmicrohttpd libzip json-c libsystemd) -lunistring -pthread
COMMON := $(BUILD_DIR)/common.o
ARCHIVE := $(BUILD_DIR)/archive.o
BINARIES := $(addprefix $(BUILD_DIR)/,koheron-api koheron-install koheron-server-init)

.PHONY: all clean
all: $(BINARIES)

$(BUILD_DIR)/koheron-api: $(BUILD_DIR)/api_main.o $(BUILD_DIR)/journal.o $(COMMON) $(ARCHIVE)

$(BUILD_DIR)/koheron-install: $(BUILD_DIR)/install_main.o $(COMMON) $(ARCHIVE)

$(BUILD_DIR)/koheron-server-init: $(BUILD_DIR)/led_main.o $(COMMON)

$(BINARIES):
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp $(BUILD_RULES) $(COMPILER_SETTINGS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(wildcard $(BUILD_DIR)/*.d)
clean:
	rm -rf $(BUILD_DIR)
