include $(dir $(lastword $(MAKEFILE_LIST)))../make/dependencies.mk
TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server

$(TMP_SERVER_PATH)/: ; @mkdir -p $@

# any object under TMP needs the dir
$(TMP_SERVER_PATH)/%.o: | $(TMP_SERVER_PATH)/

# -----------------------------------------------------------------------------
# Command lines Verbosity (V=0 quiet, V=1 verbose)
# -----------------------------------------------------------------------------

V ?= 1
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
SERVER_CPP := $(wildcard $(SERVER_PATH)/network/*.cpp) \
              $(wildcard $(SERVER_PATH)/executor/*.cpp) \
              $(wildcard $(SERVER_PATH)/utilities/*.cpp) \
              $(wildcard $(SERVER_PATH)/main/*.cpp)
SERVER_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(SERVER_CPP))))
SERVER_LIB_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/runtime/*.cpp))))

DRIVERS_HPP := $(filter %.hpp,$(DRIVERS))
export DRIVERS_HPP

DRIVERS_CPP := $(filter %.cpp,$(DRIVERS))
DRIVERS_CPP_REL := $(patsubst $(SDK_FULL_PATH)/%,%,$(DRIVERS_CPP))
DRIVERS_OBJ := $(addprefix $(TMP_SERVER_PATH)/,$(DRIVERS_CPP_REL:.cpp=.o))

# PFFFT's system-header wrapper keeps vendor warnings out of SDK builds, but
# -MMD omits its included C sources and SIMD headers. Track them explicitly.
PFFFT_DRIVER_OBJ := $(filter %/external_libs/pffft/pffft.o,$(DRIVERS_OBJ))
ifneq ($(PFFFT_DRIVER_OBJ),)
$(PFFFT_DRIVER_OBJ): $(wildcard $(SERVER_PATH)/external_libs/pffft/*.c \
    $(SERVER_PATH)/external_libs/pffft/*.h $(SERVER_PATH)/external_libs/pffft/simd/*.h)
endif

.PHONY: list_drivers
list_drivers:
	@echo $(DRIVERS)
	@echo "-------------------"
	@echo $(DRIVERS_HPP)
	@echo "-------------------"
	@echo $(DRIVERS_CPP)
	@echo "-------------------"
	@echo $(DRIVERS_OBJ)

# Driver order determines RPC IDs, so preserve it in the selection stamp.
DRIVERS_LIST_FILE := $(TMP_SERVER_PATH)/drivers.list
$(eval $(call build_settings_stamp,$(DRIVERS_LIST_FILE),$$(foreach H,$$(DRIVERS_HPP),$$(call build_shell_quote,$$(patsubst ./%,%,$$(H))))))

# -----------------------------------------------------------------------------
# Generated interface sources/objects
# -----------------------------------------------------------------------------

INTERFACE_DRIVERS_HPP := $(addprefix $(TMP_SERVER_PATH)/interface_,$(notdir $(DRIVERS_HPP)))

define RENDER_IFACE_RULES
$(TMP_SERVER_PATH)/interface_$(notdir $(1)): \
    $(1) $(SERVER_PATH)/templates/interface_driver.hpp $(MEMORY_YML) $(DRIVERS_LIST_FILE) $(MAKE_PY_SOURCES) | $(TMP_SERVER_PATH)/ $(PYTHON_REQUIREMENTS_STAMP)
	$$(call echo-cmd,tpl)
	$$(Q)$$(MAKE_PY) --render_interface $$@ $(MEMORY_YML) $(1)
endef

$(foreach H,$(DRIVERS_HPP),$(eval $(call RENDER_IFACE_RULES,$(H))))

# Aggregated interface files must be rendered AFTER all per-driver headers exist
$(TMP_SERVER_PATH)/interface_drivers.hpp: $(INTERFACE_DRIVERS_HPP)

# -----------------------------------------------------------------------------
# Memory header from YAML
# -----------------------------------------------------------------------------

$(TMP_SERVER_PATH)/memory.hpp: $(MEMORY_YML) $(SERVER_PATH)/templates/memory.hpp $(MAKE_PY_SOURCES) | $(PYTHON_REQUIREMENTS_STAMP)
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
$1: $(SERVER_PATH)/templates/$(notdir $1) $(DRIVERS_LIST_FILE) $(MEMORY_YML) $(MAKE_PY_SOURCES) | $(PYTHON_REQUIREMENTS_STAMP)
	$(Q)mkdir -p $(dir $$@)
	$$(call echo-cmd,tpl)
	$(Q)$(MAKE_PY) --render_template $$@ $(MEMORY_YML) $$<
endef

# Render rule that depends on the full set of driver headers
define _render_template_rule_full
$1: $(SERVER_PATH)/templates/$(notdir $1) $(DRIVERS_HPP) $(DRIVERS_LIST_FILE) $(MEMORY_YML) $(MAKE_PY_SOURCES) | $(PYTHON_REQUIREMENTS_STAMP)
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

SERVER_INCLUDE_DIRS = -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/external_libs -isystem /usr/include/eigen3 -I$(SDK_PATH) -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)

SERVER_CCXX = $(DOCKER) ccache $(GCC_ARCH)-g++-$(GCC_VERSION) -flto=$(N_CPUS)
SERVER_COMPILER_STAMP := $(TMP_SERVER_PATH)/.compiler-settings
$(eval $(call compiler_settings_stamp,$(SERVER_COMPILER_STAMP),$(GCC_ARCH)-g++-$(GCC_VERSION)))
$(OBJ): $(SERVER_COMPILER_STAMP)

SERVER_CCXXFLAGS = -Wall -Werror -Wextra
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized  -Wmissing-declarations
SERVER_CCXXFLAGS += -Wno-psabi -Wno-error=deprecated-declarations
SERVER_CCXXFLAGS += $(SERVER_INCLUDE_DIRS)
SERVER_CCXXFLAGS += -DKOHERON_VERSION=\"$(KOHERON_VERSION)\" -DINSTRUMENT_NAME=\"$(NAME)\" -DKOHERON_SERVER_BUILD
SERVER_CCXXFLAGS += -O3 -fno-math-errno -fno-exceptions
SERVER_CCXXFLAGS += -MMD -MP -static-libstdc++ $(GCC_FLAGS)
SERVER_CCXXFLAGS += $(SERVER_EXTRA_CCXXFLAGS)
SERVER_CCXXFLAGS += -std=c++23 -pthread -fdiagnostics-color=always

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

PCH_FLAGS_STAMP := $(TMP_PROJECT_PATH)/pch/pch.flags

$(eval $(call build_settings_stamp,$(PCH_FLAGS_STAMP),$$(call build_shell_quote,$$(PCH_CXX)) $$(call build_shell_quote,$$(PCH_CXXFLAGS))))

$(PCH_GCH): $(PCH_DST) $(PCH_FLAGS_STAMP) $(SERVER_COMPILER_STAMP)
	$(PCH_CXX) -x c++-header $(PCH_CXXFLAGS) -o $@ $<

# Use the PCH for all compilations and ensure it’s built first
SERVER_CCXXFLAGS += -Winvalid-pch -include $(PCH_DST)
$(OBJ): $(PCH_GCH)

# The PCH deliberately excludes defines; objects must track the complete flags.
SERVER_FLAGS_STAMP := $(TMP_SERVER_PATH)/.compile-settings
$(eval $(call build_settings_stamp,$(SERVER_FLAGS_STAMP),$$(call build_shell_quote,$$(SERVER_CCXX)) $$(call build_shell_quote,$$(SERVER_CCXXFLAGS))))
$(OBJ): $(SERVER_FLAGS_STAMP)

SERVER_LINK_STAMP := $(TMP_SERVER_PATH)/.link-settings
$(eval $(call build_settings_stamp,$(SERVER_LINK_STAMP),$$(call build_shell_quote,$$(cmd_link))))

# -----------------------------------------------------------------------------
# Compile objects
# -----------------------------------------------------------------------------

# --- compile rules must wait for generated headers ---
$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/network/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/executor/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/runtime/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/hardware/%.cpp | $(GEN_HEADERS)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/utilities/%.cpp | $(GEN_HEADERS)
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

$(SERVER): $(OBJ) $(SERVER_TEMPLATE_LIST) $(GEN_HEADERS) $(SERVER_FLAGS_STAMP) $(SERVER_LINK_STAMP) | $(KOHERON_SERVER_PATH)
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
JSON_CXXFLAGS := -std=c++23 -O2 -DKOHERON_SERVER_BUILD $(SERVER_INCLUDE_DIRS)
# Metadata includes the instrument's real headers. Keep their search paths and
# feature defines, without passing cross-target options to the host compiler.
# Parse shell words so split options and quoted paths keep their arguments.
JSON_CXXFLAGS += $(shell $(if $(HOST_PYTHON),$(HOST_PYTHON),python3) $(SERVER_PATH)/tools/header_flags.py $(call build_shell_quote,$(SERVER_EXTRA_CCXXFLAGS)))
JSON_CXXFLAGS += -DINSTRUMENT_NAME=\"$(NAME)\"

JSON_FLAGS_STAMP := $(TMP_SERVER_PATH)/.json-settings
$(eval $(call build_settings_stamp,$(JSON_FLAGS_STAMP),$$(call build_shell_quote,$$(DOCKER) $$(JSON_CXX)) $$(call build_shell_quote,$$(JSON_CXXFLAGS))))

$(DRIVERS_JSON_DUMP_EXE): $(DRIVERS_JSON_DUMP_CPP) $(GEN_HEADERS) $(SERVER_COMPILER_STAMP) $(JSON_FLAGS_STAMP) $(SERVER_PATH)/server.mk
	$(DOCKER) $(JSON_CXX) $(JSON_CXXFLAGS) $< -o $@

$(DRIVERS_JSON_OUT): $(DRIVERS_JSON_DUMP_EXE)
	$(DOCKER) $(if $(filter /%,$<),$<,./$<) > $@
	$(call ok,$@)

.PHONY: json_dump
json_dump: $(DRIVERS_JSON_DUMP_EXE)

.PHONY: drivers_json
drivers_json: $(DRIVERS_JSON_OUT)
