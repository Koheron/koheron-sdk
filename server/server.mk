
KOHERON_SERVER_TAG := v0.14
KOHERON_SERVER_PATH := $(TMP)/koheron-server-$(KOHERON_SERVER_TAG)
KOHERON_SERVER_TAR := $(TMP)/koheron-server-$(KOHERON_SERVER_TAG).tar.gz
KOHERON_SERVER_URL := https://github.com/Koheron/koheron-server/archive/$(KOHERON_SERVER_TAG).tar.gz

$(KOHERON_SERVER_TAR):
	mkdir -p $(@D)
	curl -L $(KOHERON_SERVER_URL) -o $@
	@echo [$@] OK

$(KOHERON_SERVER_PATH): $(KOHERON_SERVER_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	@echo [$@] OK

TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server

$(TMP_SERVER_PATH):
	@mkdir -p $@

SERVER_TEMPLATES := $(wildcard $(SERVER_PATH)/templates/*.hpp $(SERVER_PATH)/templates/*.cpp)
SERVER_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(KOHERON_SERVER_PATH)/*.cpp))))

DRIVERS := $(shell $(MAKE_PY) --drivers $(CONFIG) $(TMP_SERVER_PATH)/drivers && cat $(TMP_SERVER_PATH)/drivers)
DRIVERS_HPP := $(filter %.hpp,$(DRIVERS))
DRIVERS_CPP := $(filter %.cpp,$(DRIVERS))
DRIVERS_OBJ := $(addprefix $(TMP_SERVER_PATH)/, $(subst .cpp,.o,$(notdir $(filter %.cpp,$(DRIVERS)))))

# Generated source files
###############################################################################
INTERFACE_DRIVERS_HPP := $(addprefix $(TMP_SERVER_PATH)/interface_,$(notdir $(DRIVERS_HPP)))
INTERFACE_DRIVERS_CPP := $(subst .hpp,.cpp,$(INTERFACE_DRIVERS_HPP))
INTERFACE_DRIVERS_OBJ := $(subst .hpp,.o,$(INTERFACE_DRIVERS_HPP))

# Render driver interfaces from templates
###############################################################################

define render_interface
$(TMP_SERVER_PATH)/interface_$(notdir $1) $(TMP_SERVER_PATH)/interface_$(subst .hpp,.cpp,$(notdir $1)): \
		$1 $(SERVER_PATH)/templates/interface_driver.hpp $(SERVER_PATH)/templates/interface_driver.cpp | $(TMP_SERVER_PATH)
	$(MAKE_PY) --render_interface $(CONFIG) $$@ $1
endef
$(foreach driver,$(DRIVERS_HPP),$(eval $(call render_interface,$(driver))))

# Render other templates
###############################################################################

$(TMP_SERVER_PATH)/memory.hpp: $(MEMORY_YML)
	$(MAKE_PY) --memory_hpp $(CONFIG) $@

SERVER_TEMPLATE_LIST := $(addprefix $(TMP_SERVER_PATH)/, drivers_table.hpp drivers_json.hpp context.cpp drivers.hpp interface_drivers.hpp operations.hpp)

define render_template
$1: $(SERVER_PATH)/templates/$(notdir $1) $(DRIVERS_HPP)
	$(MAKE_PY) --render_template $(CONFIG) $$@ $$<
endef
$(foreach template,$(SERVER_TEMPLATE_LIST),$(eval $(call render_template,$(template))))

# Compile the executable with GCC
###############################################################################
CONTEXT_OBJS := $(TMP_SERVER_PATH)/context.o $(TMP_SERVER_PATH)/spi_dev.o $(TMP_SERVER_PATH)/i2c_dev.o
OBJ := $(SERVER_OBJ) $(INTERFACE_DRIVERS_OBJ) $(DRIVERS_OBJ) $(CONTEXT_OBJS)
DEP := $(subst .o,.d,$(OBJ))
-include $(DEP)

CCXX := /usr/bin/arm-linux-gnueabihf-g++ -flto

SERVER_CCXXFLAGS := -Wall -Werror -Wextra
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wcast-align -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized -Wshadow -Wzero-as-null-pointer-constant -Wmissing-declarations
# SERVER_CCXXFLAGS += -Wconversion -Wsign-conversion
SERVER_CCXXFLAGS += -I$(TMP_SERVER_PATH) -I$(KOHERON_SERVER_PATH) -I. -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)
SERVER_CCXXFLAGS += -DKOHERON_VERSION=$(KOHERON_VERSION).$(shell git rev-parse --short HEAD)
SERVER_CCXXFLAGS += -MMD -MP -O3 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard
SERVER_CCXXFLAGS += -std=c++14 -pthread

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/context/%.cpp
	$(CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(KOHERON_SERVER_PATH)/%.cpp
	$(CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(TMP_SERVER_PATH)/%.cpp
	$(CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(SERVER): $(OBJ)
	$(CCXX) -o $@ $(OBJ) $(SERVER_CCXXFLAGS) -lm

.PHONY: server
server: $(SERVER_TEMPLATE_LIST) $(INTERFACE_DRIVERS_HPP) $(INTERFACE_DRIVERS_CPP) $(TMP_SERVER_PATH)/memory.hpp | $(KOHERON_SERVER_PATH)
	$(MAKE) --jobs=$(N_CPUS) $(SERVER)

# Clean targets
###############################################################################

.PHONY: clean_server
clean_server:
	rm -rf $(TMP_SERVER_PATH)
