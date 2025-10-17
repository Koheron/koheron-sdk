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
SERVER_CPP := $(wildcard $(SERVER_PATH)/core/*.cpp) \
              $(wildcard $(SERVER_PATH)/core/drivers/*.cpp) \
              $(wildcard $(SERVER_PATH)/main/*.cpp)
SERVER_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(SERVER_CPP))))
SERVER_LIB_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/runtime/*.cpp))))

DRIVERS_HPP := $(filter %.hpp,$(DRIVERS))
export DRIVERS_HPP

DRIVERS_CPP := $(filter %.cpp,$(DRIVERS))
DRIVERS_CPP_REL := $(patsubst $(SDK_FULL_PATH)/%,%,$(DRIVERS_CPP))
DRIVERS_OBJ := $(addprefix $(TMP_SERVER_PATH)/,$(DRIVERS_CPP_REL:.cpp=.o))

.PHONY: list_drivers
list_drivers:
	@echo $(DRIVERS)
	@echo "-------------------"
	@echo $(DRIVERS_HPP)
	@echo "-------------------"
	@echo $(DRIVERS_CPP)
	@echo "-------------------"
	@echo $(DRIVERS_OBJ)

# A stamp that only changes when the *set* of drivers changes
DRIVERS_LIST_FILE := $(TMP_SERVER_PATH)/drivers.list

# Update the list only if content actually changed (keeps timestamp stable)
$(DRIVERS_LIST_FILE): | $(TMP_SERVER_PATH)
	@{ \
	  printf "%s\n" $(DRIVERS_HPP) | sed 's|^\./||' | LC_ALL=C sort; \
	} > $@.tmp
	@cmp -s $@.tmp $@ || mv -f $@.tmp $@
	@rm -f $@.tmp

# -----------------------------------------------------------------------------
# Generated interface sources/objects
# -----------------------------------------------------------------------------

INTERFACE_DRIVERS_HPP := $(addprefix $(TMP_SERVER_PATH)/interface_,$(notdir $(DRIVERS_HPP)))

define RENDER_IFACE_RULES
$(TMP_SERVER_PATH)/interface_$(notdir $(1)): \
    $(1) $(SERVER_PATH)/templates/interface_driver.hpp | $(TMP_SERVER_PATH)
	$$(call echo-cmd,tpl)
	$$(Q)$$(MAKE_PY) --render_interface $$@ $(MEMORY_YML) $(1)
endef

$(foreach H,$(DRIVERS_HPP),$(eval $(call RENDER_IFACE_RULES,$(H))))

# Aggregated interface files must be rendered AFTER all per-driver headers exist
$(TMP_SERVER_PATH)/interface_drivers.hpp: $(INTERFACE_DRIVERS_HPP)

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

# Aggregates that depend only on the membership of DRIVERS (not their contents)
AGG_LIST_TEMPLATES := $(addprefix $(TMP_SERVER_PATH)/, \
  drivers_list.hpp interface_drivers.hpp)

# Aggregates that depend on the actual driver *contents*
META_TEMPLATES := $(addprefix $(TMP_SERVER_PATH)/, \
  drivers_json.hpp drivers.hpp operations.hpp)

# Render rule that depends on the list *only*
define _render_template_rule_list
$1: $(SERVER_PATH)/templates/$(notdir $1) $(DRIVERS_LIST_FILE)
	$(Q)mkdir -p $(dir $$@)
	$$(call echo-cmd,tpl)
	$(Q)$(MAKE_PY) --render_template $$@ $(MEMORY_YML) $$<
endef

# Render rule that depends on the full set of driver headers
define _render_template_rule_full
$1: $(SERVER_PATH)/templates/$(notdir $1) $(DRIVERS_HPP)
	$(Q)mkdir -p $(dir $$@)
	$$(call echo-cmd,tpl)
	$(Q)$(MAKE_PY) --render_template $$@ $(MEMORY_YML) $$<
endef

$(foreach t,$(AGG_LIST_TEMPLATES),$(eval $(call _render_template_rule_list,$(t))))
$(foreach t,$(META_TEMPLATES),$(eval $(call _render_template_rule_full,$(t))))

# -----------------------------------------------------------------------------
# Objects
# -----------------------------------------------------------------------------

HARDWARE_OBJ := $(TMP_SERVER_PATH)/spi_manager.o $(TMP_SERVER_PATH)/i2c_manager.o $(TMP_SERVER_PATH)/fpga_manager.o $(TMP_SERVER_PATH)/zynq_fclk.o
OBJ := $(SERVER_OBJ) $(SERVER_LIB_OBJ) $(DRIVERS_OBJ) $(HARDWARE_OBJ)
DEP := $(subst .o,.d,$(OBJ))
-include $(DEP)

# -----------------------------------------------------------------------------
# Compiler
# -----------------------------------------------------------------------------

SERVER_INCLUDE_DIRS = -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/external_libs -I$(SERVER_PATH)/core -I$(SERVER_PATH)/hardware -I$(SDK_PATH) -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)

SERVER_CCXX = $(DOCKER) ccache $(GCC_ARCH)-g++-$(GCC_VERSION) -flto=$(N_CPUS)

SERVER_CCXXFLAGS = -Wall -Werror -Wextra
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized  -Wmissing-declarations
SERVER_CCXXFLAGS += -Wno-psabi -Wno-error=deprecated-declarations
SERVER_CCXXFLAGS += $(SERVER_INCLUDE_DIRS)
SERVER_CCXXFLAGS += -DKOHERON_VERSION=\"$(KOHERON_VERSION).$(shell git rev-parse --short HEAD)\" -DINSTRUMENT_NAME=\"$(NAME)\" -DKOHERON_SERVER_BUILD
SERVER_CCXXFLAGS += -O3 -fno-math-errno -fno-exceptions
SERVER_CCXXFLAGS += -MMD -MP -static-libstdc++ $(GCC_FLAGS)
SERVER_CCXXFLAGS += -std=c++20 -pthread

.PHONY: gcc_flags
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

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/core/drivers/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/runtime/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/hardware/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/main/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
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

$(SERVER): $(OBJ) $(SERVER_TEMPLATE_LIST) $(GEN_HEADERS) | $(KOHERON_SERVER_PATH)
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

# -----------------------------------------------------------------------------
# Dump JSON
# -----------------------------------------------------------------------------

DRIVERS_JSON_HPP      := $(TMP_SERVER_PATH)/drivers_json.hpp
INTERFACE_DRIVERS_HPP := $(TMP_SERVER_PATH)/interface_drivers.hpp

DRIVERS_JSON_DUMP_CPP := $(SERVER_PATH)/tools/drivers_json_dump.cpp
DRIVERS_JSON_DUMP_EXE := $(TMP_SERVER_PATH)/drivers_json_dump
DRIVERS_JSON_OUT      := $(TMP_SERVER_PATH)/drivers.json

JSON_CXX      := g++-$(GCC_VERSION)
JSON_CXXFLAGS := -std=c++20 -O2 -DKOHERON_SERVER_BUILD $(SERVER_INCLUDE_DIRS)

$(DRIVERS_JSON_DUMP_EXE): $(DRIVERS_JSON_DUMP_CPP) $(DRIVERS_JSON_HPP) $(INTERFACE_DRIVERS_HPP)
	$(DOCKER) $(JSON_CXX) $(JSON_CXXFLAGS) $< -o $@

$(DRIVERS_JSON_OUT): $(DRIVERS_JSON_DUMP_EXE)
	./$< > $@
	$(call ok,$@)

.PHONY: json_dump
json_dump: $(DRIVERS_JSON_DUMP_EXE)

.PHONY: drivers_json
drivers_json: $(DRIVERS_JSON_OUT)
