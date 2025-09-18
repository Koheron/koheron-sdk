
TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server

$(TMP_SERVER_PATH)/%.o: | $(TMP_SERVER_PATH)/

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

$(TMP_SERVER_PATH)/interface_drivers.hpp: $(INTERFACE_DRIVERS_HPP)
$(TMP_SERVER_PATH)/interface_drivers.cpp: $(INTERFACE_DRIVERS_HPP)

# Render driver interfaces from templates
###############################################################################

$(foreach H,$(DRIVERS_HPP),\
  $(eval $(TMP_SERVER_PATH)/interface_$(notdir $(H)): \
      $(H) $(SERVER_PATH)/templates/interface_driver.hpp $(SERVER_PATH)/templates/interface_driver.cpp | $(TMP_SERVER_PATH)/ ; \
      $$(MAKE_PY) --render_interface $(CONFIG) $$@ $(H)) \
  $(eval $(TMP_SERVER_PATH)/interface_$(patsubst %.hpp,%.cpp,$(notdir $(H))): \
      $(H) $(SERVER_PATH)/templates/interface_driver.hpp $(SERVER_PATH)/templates/interface_driver.cpp | $(TMP_SERVER_PATH)/ ; \
      $$(MAKE_PY) --render_interface $(CONFIG) $$@ $(H)) \
)

# Render other templates
###############################################################################

$(TMP_SERVER_PATH)/memory.hpp: $(MEMORY_YML)
	$(MAKE_PY) --memory_hpp $(CONFIG) $@

SERVER_TEMPLATE_LIST := $(addprefix $(TMP_SERVER_PATH)/, drivers_table.hpp drivers_json.hpp drivers.hpp interface_drivers.hpp operations.hpp)

define render_template
$1: $(SERVER_PATH)/templates/$(notdir $1) $(DRIVERS_HPP)
	$(MAKE_PY) --render_template $(CONFIG) $$@ $$<
endef
$(foreach template,$(SERVER_TEMPLATE_LIST),$(eval $(call render_template,$(template))))

# All interface outputs (per-driver + aggregate)
INTERFACE_TARGETS := \
  $(INTERFACE_DRIVERS_HPP) $(INTERFACE_DRIVERS_CPP) \
  $(TMP_SERVER_PATH)/interface_drivers.hpp $(TMP_SERVER_PATH)/interface_drivers.cpp

# A single stamp that flips only when every interface is present
INTERFACE_STAMP := $(TMP_SERVER_PATH)/.interfaces.stamp
$(INTERFACE_STAMP): $(INTERFACE_TARGETS)
	@touch $@

# Compile the executable with GCC
###############################################################################
CONTEXT_OBJS := $(TMP_SERVER_PATH)/spi_dev.o $(TMP_SERVER_PATH)/i2c_dev.o
OBJ := $(SERVER_OBJ) $(INTERFACE_DRIVERS_OBJ) $(DRIVERS_OBJ) $(CONTEXT_OBJS)
DEP := $(subst .o,.d,$(OBJ))
-include $(DEP)

SERVER_CCXX := /usr/bin/arm-linux-gnueabihf-g++-$(GCC_VERSION) -flto=$(N_CPUS)
ifeq ($(BUILD_METHOD),docker)
	SERVER_CCXX = $(DOCKER) $(GCC_ARCH)-g++-$(GCC_VERSION) -flto=$(N_CPUS)
endif

SERVER_CCXXFLAGS = -Wall -Werror -Wextra
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS +=  -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized  -Wmissing-declarations
SERVER_CCXXFLAGS += -Wno-psabi
# SERVER_CCXXFLAGS += -Wconversion -Wsign-conversion
# SERVER_CCXXFLAGS += -Wshift-negative-value -Wduplicated-cond -Wduplicated-branches -Waligned-new
SERVER_CCXXFLAGS += -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/external_libs -I$(SERVER_PATH)/core -I$(SDK_PATH) -I. -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)
SERVER_CCXXFLAGS += -DKOHERON_VERSION=$(KOHERON_VERSION).$(shell git rev-parse --short HEAD)
SERVER_CCXXFLAGS += -O3 -fno-math-errno
SERVER_CCXXFLAGS += -MMD -MP -static-libstdc++ $(GCC_FLAGS)
SERVER_CCXXFLAGS += -std=c++20 -pthread

PHONY: gcc_flags
gcc_flags:
	@echo $(GCC_FLAGS)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/context/%.cpp $(INTERFACE_STAMP)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/core/%.cpp $(INTERFACE_STAMP)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(TMP_SERVER_PATH)/%.cpp $(INTERFACE_STAMP)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(SERVER): $(OBJ) $(INTERFACE_STAMP)
	$(SERVER_CCXX) -o $@ $(OBJ) $(SERVER_CCXXFLAGS) -lm

.PHONY: server
server: $(SERVER_TEMPLATE_LIST) $(INTERFACE_DRIVERS_HPP) $(INTERFACE_DRIVERS_CPP) $(TMP_SERVER_PATH)/memory.hpp | $(KOHERON_SERVER_PATH)
	$(MAKE) --jobs=$(N_CPUS) $(SERVER)

# Clean targets
###############################################################################

.PHONY: clean_server
clean_server:
	rm -rf $(TMP_SERVER_PATH)
