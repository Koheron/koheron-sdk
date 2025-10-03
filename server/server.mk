TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server

# ensure tmp dir exists early
$(shell mkdir -p '$(TMP_SERVER_PATH)' >/dev/null 2>&1)
$(TMP_SERVER_PATH)/: ; @mkdir -p $@

# any object under TMP needs the dir
$(TMP_SERVER_PATH)/%.o: | $(TMP_SERVER_PATH)/

# -----------------------------------------------------------------------------
# Command lines Verbosity (V=0 quiet, V=1 verbose)
# -----------------------------------------------------------------------------

V ?= 0
ifeq ($(V),1)
  Q :=
  echo-cmd =
else
  Q := @
  echo-cmd = @printf "  %s\n" "$($(quiet)cmd_$(1))";
endif
quiet := quiet_

# helpers: expand a cmd variable by name
cmd = $($(1))

# pretty label shown when V=0
quiet_cmd_cxx = CPP $@

# the real compile command
cmd_cxx = $(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

# link
quiet_cmd_link = LD  $@
cmd_link = $(SERVER_CCXX) -o $@ $(OBJ) $(SERVER_CCXXFLAGS) -lm

# Pretty printer for template rendering
quiet_cmd_tpl = TPL $@

# -----------------------------------------------------------------------------
# Discover
# -----------------------------------------------------------------------------

SERVER_TEMPLATES := $(wildcard $(SERVER_PATH)/templates/*.hpp $(SERVER_PATH)/templates/*.cpp)
SERVER_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/core/*.cpp))))
SERVER_LIB_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/runtime/*.cpp))))

DRIVERS_HPP := $(filter %.hpp,$(DRIVERS))
export DRIVERS_HPP

DRIVERS_CPP := $(filter %.cpp,$(DRIVERS))
DRIVERS_CPP_REL := $(patsubst $(SDK_FULL_PATH)/%,%,$(DRIVERS_CPP))
DRIVERS_OBJ := $(addprefix $(TMP_SERVER_PATH)/,$(DRIVERS_CPP_REL:.cpp=.o))

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

define RENDER_IFACE_RULES
$(TMP_SERVER_PATH)/interface_$(notdir $(1)): \
    $(1) $(SERVER_PATH)/templates/interface_driver.hpp $(SERVER_PATH)/templates/interface_driver.cpp | $(TMP_SERVER_PATH)
	$$(call echo-cmd,tpl)
	$$(Q)$$(MAKE_PY) --render_interface $$@ $(MEMORY_YML) $(1)

$(TMP_SERVER_PATH)/interface_$(patsubst %.hpp,%.cpp,$(notdir $(1))): \
    $(1) $(SERVER_PATH)/templates/interface_driver.hpp $(SERVER_PATH)/templates/interface_driver.cpp | $(TMP_SERVER_PATH)
	$$(call echo-cmd,tpl)
	$$(Q)$$(MAKE_PY) --render_interface $$@ $(MEMORY_YML) $(1)
endef

$(foreach H,$(DRIVERS_HPP),$(eval $(call RENDER_IFACE_RULES,$(H))))

# Aggregated interface files must be rendered AFTER all per-driver headers exist
$(TMP_SERVER_PATH)/interface_drivers.hpp: $(INTERFACE_DRIVERS_HPP)
$(TMP_SERVER_PATH)/interface_drivers.cpp: $(INTERFACE_DRIVERS_HPP)

# -----------------------------------------------------------------------------
# Memory header from YAML
# -----------------------------------------------------------------------------

$(TMP_SERVER_PATH)/memory.hpp: $(MEMORY_YML) $(SERVER_PATH)/templates/memory.hpp
	$(Q)mkdir -p $(dir $@)
	@$(call echo-cmd,tpl)
	$(Q)$(MAKE_PY) --memory_hpp $@ $(MEMORY_YML)

# -----------------------------------------------------------------------------
# Other templates
# -----------------------------------------------------------------------------
SERVER_TEMPLATE_LIST := $(addprefix $(TMP_SERVER_PATH)/, \
  drivers_list.hpp drivers_json.hpp drivers.hpp interface_drivers.hpp operations.hpp get_driver_inst.cpp)

define _render_template_rule
$1: $(SERVER_PATH)/templates/$(notdir $1) $(DRIVERS_HPP)
	$(Q)mkdir -p $(dir $$@)
	$$(call echo-cmd,tpl)
	$(Q)$(MAKE_PY) --render_template $$@ $(MEMORY_YML) $$<
endef

$(foreach template,$(SERVER_TEMPLATE_LIST),$(eval $(call _render_template_rule,$(template))))

INTERFACE_DRIVERS_OBJ += $(TMP_SERVER_PATH)/get_driver_inst.o

# -----------------------------------------------------------------------------
# Objects
# -----------------------------------------------------------------------------

CONTEXT_OBJS := $(TMP_SERVER_PATH)/spi_dev.o $(TMP_SERVER_PATH)/i2c_dev.o $(TMP_SERVER_PATH)/fpga_manager.o $(TMP_SERVER_PATH)/zynq_fclk.o
OBJ := $(SERVER_OBJ) $(SERVER_LIB_OBJ) $(INTERFACE_DRIVERS_OBJ) $(DRIVERS_OBJ) $(CONTEXT_OBJS)
DEP := $(subst .o,.d,$(OBJ))
-include $(DEP)

# -----------------------------------------------------------------------------
# Compiler
# -----------------------------------------------------------------------------

SERVER_CCXX = $(DOCKER) ccache $(GCC_ARCH)-g++-$(GCC_VERSION) -flto=$(N_CPUS)

SERVER_CCXXFLAGS = -Wall -Werror -Wextra
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized  -Wmissing-declarations
SERVER_CCXXFLAGS += -Wno-psabi
SERVER_CCXXFLAGS += -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/external_libs -I$(SERVER_PATH)/core -I$(SDK_PATH) -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)
SERVER_CCXXFLAGS += -DKOHERON_VERSION=\"$(KOHERON_VERSION).$(shell git rev-parse --short HEAD)\" -DINSTRUMENT_NAME=\"$(NAME)\" -DKOHERON_SERVER_BUILD
SERVER_CCXXFLAGS += -O3 -fno-math-errno -fno-exceptions
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

# -----------------------------------------------------------------------------
# Precompiled headers
# -----------------------------------------------------------------------------

PCH_SRC := $(SERVER_PATH)/pch.hpp
PCH_DST := $(TMP_PROJECT_PATH)/pch/pch.hpp
PCH_GCH := $(PCH_DST).gch

PCH_CXX := $(DOCKER) ccache $(GCC_ARCH)-g++-$(GCC_VERSION)
PCH_CXXFLAGS := $(filter-out -flto% -static-libstdc++ -MMD -MP,$(SERVER_CCXXFLAGS))
PCH_CXXFLAGS := $(filter-out -D%,$(PCH_CXXFLAGS))
PCH_CXXFLAGS += -Wno-unused-macros

$(PCH_DST): $(PCH_SRC)
	@mkdir -p $(dir $@)
	@cp -f $< $@

$(PCH_GCH): $(PCH_DST)
	$(PCH_CXX) -x c++-header $(PCH_CXXFLAGS) -o $@ $<

# Use the PCH for all compilations and ensure it’s built first
SERVER_CCXXFLAGS += -Winvalid-pch -include $(PCH_DST)
$(OBJ): | $(PCH_GCH)

# -----------------------------------------------------------------------------
# Compile objects
# -----------------------------------------------------------------------------

# --- compile rules must wait for generated headers ---
$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/core/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/runtime/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/context/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

# Generated interface .cpp => .o (depends on its own .hpp via auto-deps)
$(TMP_SERVER_PATH)/%.o: $(TMP_SERVER_PATH)/%.cpp | $(GEN_HEADERS)
	@$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: %.cpp | $(GEN_HEADERS)
	@mkdir -p $(dir $@)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

# -----------------------------------------------------------------------------
# Link
# -----------------------------------------------------------------------------

# ---- Derived fields for summary ----
STD_FLAG   := $(firstword $(filter -std=%,$(SERVER_CCXXFLAGS)))
OPT_FLAG   := $(firstword $(filter -O%,$(SERVER_CCXXFLAGS)))
HAS_PTHREAD:= $(if $(findstring -pthread,$(SERVER_CCXXFLAGS)),yes,no)

INC_DIRS   := $(patsubst -I%,%,$(filter -I%,$(SERVER_CCXXFLAGS)))
INC_COUNT  := $(words $(INC_DIRS))

DEF_LIST   := $(patsubst -D%,%,$(filter -D%,$(SERVER_CCXXFLAGS)))
DEF_COUNT  := $(words $(DEF_LIST))

# Pretty tag + real command for summary
quiet_cmd_cfg = CFG $@
cmd_cfg = \
  printf "  Toolchain : %s\n" '$(GCC_ARCH)-g++-$(GCC_VERSION)'; \
  printf "  Standard  : %s   Optimize: %s   LTO: %s   Threads: %s   Pthreads: %s\n" \
         '$(STD_FLAG)' '$(OPT_FLAG)' '$(firstword $(filter -flto%,$(SERVER_CCXX)))' '$(N_CPUS)' '$(HAS_PTHREAD)'; \
  printf "  Includes  : %s dirs\n" '$(INC_COUNT)'; \
  set -- $(INC_DIRS); for d in $$@; do \
    printf "              %s\n" "$$d"; \
  done; \
  printf "  Defines   : %s\n" '$(DEF_COUNT)'; \
  set -- $(DEF_LIST); for d in $$@; do \
    printf "              %s\n" "$$d"; \
  done; \
  :

$(SERVER): $(OBJ) $(SERVER_TEMPLATE_LIST) $(GEN_HEADERS) $(INTERFACE_DRIVERS_CPP) | $(KOHERON_SERVER_PATH)
	@$(call start,$@)
	@$(call echo-cmd,link)
	$(Q)$(call cmd,cmd_link)
	@$(call ok,$@)

.PHONY: server_config
server_config:
	@$(call echo-cmd,cfg)
	$(Q)$(call cmd,cmd_cfg)

.PHONY: server
server: ccache-prepare
	@$(call echo-cmd,cfg_server)
	$(Q)$(call cmd,cmd_cfg)
	+$(Q)$(MAKE) --no-print-directory $(SERVER)

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------

.PHONY: clean_server
clean_server:
	rm -rf $(TMP_SERVER_PATH)
