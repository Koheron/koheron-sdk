# Shared setup for the RPC server and custom applications. Set variant-specific
# include directories, compiler cache, warning flags and build flags before this.
SERVER_BUILD_PATH := $(dir $(lastword $(MAKEFILE_LIST)))
include $(SERVER_BUILD_PATH)../make/dependencies.mk
include $(SERVER_BUILD_PATH)../os/compiler-settings.mk

TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server
$(TMP_SERVER_PATH)/: ; @mkdir -p $@

V ?= 1
ifeq ($(V),1)
  Q :=
  echo-cmd =
else
  Q := @
  echo-cmd = @printf "  %s\n" "$($(quiet)cmd_$(1))";
endif
quiet := quiet_
cmd = $($(1))
quiet_cmd_cxx = CPP $@
cmd_cxx = $(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<
quiet_cmd_link = LD  $@
cmd_link = $(SERVER_CCXX) -o $@ $(OBJ) $(SERVER_CCXXFLAGS) -lm
quiet_cmd_tpl = TPL $@

SERVER_INCLUDE_DIRS ?= $(if $(APP_PATH),-I$(APP_PATH)) -I$(TMP_SERVER_PATH) \
    -I$(SERVER_PATH)/external_libs -I$(SERVER_PATH)/runtime -I$(SERVER_PATH)/hardware \
    -I$(SDK_PATH) -I. -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH) \
    -isystem /usr/include/eigen3
SERVER_CCXX = $(DOCKER) $(SERVER_COMPILER_CACHE) $(GCC_ARCH)-g++-$(GCC_VERSION) -flto=$(N_CPUS)
SERVER_CCXXFLAGS = -Wall -Wextra $(SERVER_WARNING_FLAGS)
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized -Wmissing-declarations -Wno-psabi
SERVER_CCXXFLAGS += $(SERVER_INCLUDE_DIRS) -DINSTRUMENT_NAME=\"$(NAME)\"
SERVER_CCXXFLAGS += -O3 -fno-math-errno -MMD -MP -static-libstdc++ $(GCC_FLAGS)
SERVER_CCXXFLAGS += $(SERVER_BUILD_FLAGS) $(SERVER_EXTRA_CCXXFLAGS)
SERVER_CCXXFLAGS += -std=c++23 -pthread

SERVER_COMPILER_STAMP := $(TMP_SERVER_PATH)/.compiler-settings
$(eval $(call compiler_settings_stamp,$(SERVER_COMPILER_STAMP),$(GCC_ARCH)-g++-$(GCC_VERSION)))

$(TMP_SERVER_PATH)/memory.hpp: $(MEMORY_YML) $(SERVER_PATH)/templates/memory.hpp $(MAKE_PY_SOURCES) | $(TMP_SERVER_PATH)/ $(PYTHON_REQUIREMENTS_STAMP)
	$(call echo-cmd,tpl)
	$(Q)$(MAKE_PY) --memory_hpp $@ $(MEMORY_YML)
