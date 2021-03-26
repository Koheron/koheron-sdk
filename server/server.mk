
TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server

$(TMP_SERVER_PATH):
	@mkdir -p $@

SERVER_TEMPLATES := $(wildcard $(SERVER_PATH)/templates/*.hpp $(SERVER_PATH)/templates/*.cpp)
SERVER_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/core/*.cpp))))

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

SERVER_CCXX := /usr/bin/arm-linux-gnueabihf-g++-$(GCC_VERSION) -flto

SERVER_CCXXFLAGS := -Wall -Werror -Wextra
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wcast-align -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized -Wshadow -Wzero-as-null-pointer-constant -Wmissing-declarations
# SERVER_CCXXFLAGS += -Wconversion -Wsign-conversion
SERVER_CCXXFLAGS += -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/core -I$(SDK_PATH) -I. -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)
SERVER_CCXXFLAGS += -DKOHERON_VERSION=$(KOHERON_VERSION).$(shell git rev-parse --short HEAD)
SERVER_CCXXFLAGS += -MMD -MP -O3 $(GCC_FLAGS)
# Arch flags obtain by running on the Zynq:
# gcc -march=native -Q --help=target
SERVER_CCXXFLAGS += -mcpu=cortex-a9 -mfpu=vfpv3-d16 -mvectorize-with-neon-quad -mfloat-abi=hard
SERVER_CCXXFLAGS += -std=c++17 -pthread -lstdc++ -lstdc++fs -static-libstdc++

PHONY: gcc_flags
gcc_flags:
	@echo $(GCC_FLAGS)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/context/%.cpp
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/core/%.cpp
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(TMP_SERVER_PATH)/%.cpp
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(SERVER): $(OBJ)
	$(SERVER_CCXX) -o $@ $(OBJ) $(SERVER_CCXXFLAGS) -lm

.PHONY: server
server: $(SERVER_TEMPLATE_LIST) $(INTERFACE_DRIVERS_HPP) $(INTERFACE_DRIVERS_CPP) $(TMP_SERVER_PATH)/memory.hpp | $(KOHERON_SERVER_PATH)
	$(MAKE) --jobs=$(N_CPUS) $(SERVER)

# Clean targets
###############################################################################

.PHONY: clean_server
clean_server:
	rm -rf $(TMP_SERVER_PATH)
