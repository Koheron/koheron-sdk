SERVER_COMPILER_CACHE := ccache
SERVER_WARNING_FLAGS := -Werror -Wno-error=deprecated-declarations
SERVER_BUILD_FLAGS = -DKOHERON_VERSION=\"$(KOHERON_VERSION)\" -DKOHERON_SERVER_BUILD -fno-exceptions -fdiagnostics-color=always
SERVER_INCLUDE_DIRS = -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/external_libs -isystem /usr/include/eigen3 -I$(SDK_PATH) -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)
include $(dir $(lastword $(MAKEFILE_LIST)))build-common.mk

# -----------------------------------------------------------------------------
# Discover
# -----------------------------------------------------------------------------

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
# Other templates
# -----------------------------------------------------------------------------

# Aggregates that depend only on the membership of DRIVERS (not their contents)
AGG_LIST_TEMPLATES := $(addprefix $(TMP_SERVER_PATH)/, \
  drivers_list.hpp interface_drivers.hpp)

# Aggregates that depend on the actual driver *contents*
META_TEMPLATES := $(addprefix $(TMP_SERVER_PATH)/, \
  drivers_json.hpp drivers.hpp operations.hpp)

$(AGG_LIST_TEMPLATES) $(META_TEMPLATES): $(TMP_SERVER_PATH)/%: $(SERVER_PATH)/templates/% $(DRIVERS_LIST_FILE) $(MEMORY_YML) $(MAKE_PY_SOURCES) | $(TMP_SERVER_PATH)/ $(PYTHON_REQUIREMENTS_STAMP)
	$(call echo-cmd,tpl)
	$(Q)$(MAKE_PY) --render_template $@ $(MEMORY_YML) $<

$(META_TEMPLATES): $(DRIVERS_HPP)

# -----------------------------------------------------------------------------
# Objects
# -----------------------------------------------------------------------------

HARDWARE_OBJ := $(TMP_SERVER_PATH)/spi_manager.o $(TMP_SERVER_PATH)/i2c_manager.o $(TMP_SERVER_PATH)/fpga_manager.o $(TMP_SERVER_PATH)/zynq_fclk.o
OBJ := $(SERVER_OBJ) $(SERVER_LIB_OBJ) $(DRIVERS_OBJ) $(HARDWARE_OBJ)

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

# Track complete object flags after adding the PCH options.
include $(SERVER_BUILD_PATH)build-rules.mk

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
