TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server

# ensure tmp dir exists early
$(shell mkdir -p '$(TMP_SERVER_PATH)' >/dev/null 2>&1)
$(TMP_SERVER_PATH)/: ; @mkdir -p $@

# any object under TMP needs the dir
$(TMP_SERVER_PATH)/%.o: | $(TMP_SERVER_PATH)/

# -----------------------------------------------------------------------------
# Discover
# -----------------------------------------------------------------------------
SERVER_TEMPLATES := $(wildcard $(SERVER_PATH)/templates/*.hpp $(SERVER_PATH)/templates/*.cpp)
SERVER_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/core/*.cpp))))
SERVER_LIB_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/runtime/*.cpp))))

export DRIVERS
# override DRIVERS := $(abspath $(DRIVERS))

# DRIVERS_HPP := $(filter %.hpp,$(DRIVERS))
# override DRIVERS := $(abspath $(DRIVERS_HPP))

override PROJECT_PATH := $(abspath $(PROJECT_PATH))
override DRIVERS      := $(abspath $(DRIVERS))

DRIVERS_HPP := $(filter %.hpp,$(DRIVERS))
DRIVERS_CPP := $(filter %.cpp,$(DRIVERS))

# Strip the PROJECT_PATH prefix, keep subdirs, then map to TMP_SERVER_PATH and .o
DRIVERS_OBJ := $(addprefix $(TMP_SERVER_PATH)/, \
                 $(patsubst $(PROJECT_PATH)/%,%,$(DRIVERS_CPP:.cpp=.o)))

# DRIVERS_CPP := $(filter %.cpp,$(DRIVERS))
# DRIVERS_OBJ := $(addprefix $(TMP_SERVER_PATH)/, $(subst .cpp,.o,$(notdir $(filter %.cpp,$(DRIVERS)))))

PHONY: list_drivers
list_drivers:
	@echo $(DRIVERS)
	@echo "-------------------"
	@echo $(DRIVERS_HPP)
	@echo "-------------------"
	@echo $(DRIVERS_CPP)
	@echo "-------------------"
	@echo $(DRIVERS_OBJ)

# -----------------------------------------------------------------------------
# Generated interface sources/objects
# -----------------------------------------------------------------------------
INTERFACE_DRIVERS_HPP := $(addprefix $(TMP_SERVER_PATH)/interface_,$(notdir $(DRIVERS_HPP)))
INTERFACE_DRIVERS_CPP := $(subst .hpp,.cpp,$(INTERFACE_DRIVERS_HPP))
INTERFACE_DRIVERS_OBJ := $(subst .hpp,.o,$(INTERFACE_DRIVERS_HPP))

# For each driver header H, make two explicit targets (hpp & cpp)
$(foreach H,$(DRIVERS_HPP),\
  $(eval $(TMP_SERVER_PATH)/interface_$(notdir $(H)): \
      $(H) $(SERVER_PATH)/templates/interface_driver.hpp $(SERVER_PATH)/templates/interface_driver.cpp | $(TMP_SERVER_PATH)/ ; \
      $$(MAKE_PY) --render_interface $$@ $(MEMORY_YML) $(H)) \
  $(eval $(TMP_SERVER_PATH)/interface_$(patsubst %.hpp,%.cpp,$(notdir $(H))): \
      $(H) $(SERVER_PATH)/templates/interface_driver.hpp $(SERVER_PATH)/templates/interface_driver.cpp | $(TMP_SERVER_PATH)/ ; \
      $$(MAKE_PY) --render_interface $$@ $(MEMORY_YML) $(H)) \
)

# Aggregated interface files must be rendered AFTER all per-driver headers exist
$(TMP_SERVER_PATH)/interface_drivers.hpp: $(INTERFACE_DRIVERS_HPP)
$(TMP_SERVER_PATH)/interface_drivers.cpp: $(INTERFACE_DRIVERS_HPP)

# -----------------------------------------------------------------------------
# Memory header from YAML
# -----------------------------------------------------------------------------
$(TMP_SERVER_PATH)/memory.hpp: $(MEMORY_YML) $(SERVER_PATH)/templates/memory.hpp
	$(MAKE_PY) --memory_hpp $@ $(MEMORY_YML)

# -----------------------------------------------------------------------------
# Other templates
# -----------------------------------------------------------------------------
SERVER_TEMPLATE_LIST := $(addprefix $(TMP_SERVER_PATH)/, \
  drivers_list.hpp drivers_json.hpp drivers.hpp interface_drivers.hpp operations.hpp)

define _render_template_rule
$1: $(SERVER_PATH)/templates/$(notdir $1) $(DRIVERS_HPP)
	$(MAKE_PY) --render_template $$@ $(MEMORY_YML) $$<
endef
$(foreach template,$(SERVER_TEMPLATE_LIST),$(eval $(call _render_template_rule,$(template))))

# -----------------------------------------------------------------------------
# Compile / Link
# -----------------------------------------------------------------------------
CONTEXT_OBJS := $(TMP_SERVER_PATH)/spi_dev.o $(TMP_SERVER_PATH)/i2c_dev.o
OBJ := $(SERVER_OBJ) $(SERVER_LIB_OBJ) $(INTERFACE_DRIVERS_OBJ) $(DRIVERS_OBJ) $(CONTEXT_OBJS)
DEP := $(subst .o,.d,$(OBJ))
-include $(DEP)

SERVER_CCXX = $(DOCKER) $(GCC_ARCH)-g++-$(GCC_VERSION) -flto=$(N_CPUS)

SERVER_CCXXFLAGS = -Wall -Werror -Wextra
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized  -Wmissing-declarations
SERVER_CCXXFLAGS += -Wno-psabi
SERVER_CCXXFLAGS += -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/external_libs -I$(SERVER_PATH)/core -I$(SDK_PATH) -I. -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)
SERVER_CCXXFLAGS += -DKOHERON_VERSION=$(KOHERON_VERSION).$(shell git rev-parse --short HEAD) -DINSTRUMENT_NAME=$(NAME) -DKOHERON_SERVER_BUILD
SERVER_CCXXFLAGS += -O3 -fno-math-errno
SERVER_CCXXFLAGS += -MMD -MP -static-libstdc++ $(GCC_FLAGS)
SERVER_CCXXFLAGS += -std=c++20 -pthread

PHONY: gcc_flags
gcc_flags:
	@echo $(GCC_FLAGS)

GEN_HEADERS := \
  $(TMP_SERVER_PATH)/memory.hpp \
  $(TMP_SERVER_PATH)/drivers.hpp \
  $(TMP_SERVER_PATH)/drivers_json.hpp \
  $(TMP_SERVER_PATH)/drivers_list.hpp \
  $(TMP_SERVER_PATH)/interface_drivers.hpp \
  $(INTERFACE_DRIVERS_HPP)

# --- compile rules must wait for generated headers ---
$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/core/%.cpp | $(GEN_HEADERS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/runtime/%.cpp | $(GEN_HEADERS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/context/%.cpp | $(GEN_HEADERS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(TMP_SERVER_PATH)/%.cpp | $(GEN_HEADERS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(PROJECT_PATH)/%.cpp | $(GEN_HEADERS)
	@mkdir -p $(dir $@)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

# Generated interface .cpp => .o (depends on its own .hpp via auto-deps)
$(TMP_SERVER_PATH)/%.o: $(TMP_SERVER_PATH)/%.cpp | $(GEN_HEADERS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

# Link: depend on actual generated files (not stamps)
$(SERVER): $(OBJ) $(SERVER_TEMPLATE_LIST) $(GEN_HEADERS) $(INTERFACE_DRIVERS_CPP) | $(KOHERON_SERVER_PATH)
	@$(call start,$@)
	$(SERVER_CCXX) -o $@ $(OBJ) $(SERVER_CCXXFLAGS) -lm
	@$(call ok,$@)

.PHONY: server
server: $(SERVER)
	$(MAKE) --jobs=$(N_CPUS) $(SERVER)

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------
.PHONY: clean_server
clean_server:
	rm -rf $(TMP_SERVER_PATH)
