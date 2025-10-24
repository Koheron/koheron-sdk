# Cortex-R5 firmware build helper

# Guard: only do work if any sources are defined
ifeq (,$(strip $(RPU_SRCS)$(RPU_CPP_SRCS)$(RPU_ASM_SRCS)))
# Nothing to build for the RPU; provide empty extension point.
else

RPU_ELF_NAME ?= $(NAME)-r5.elf
RPU_INSTALL_PATH ?= $(RPU_ELF_NAME)
RPU_REMOTEPROC ?=
RPU_OUTPUT_DIR := $(TMP_PROJECT_PATH)/firmware
RPU_OBJ_DIR    := $(RPU_OUTPUT_DIR)/obj
RPU_ENTRY_FILE := $(RPU_OUTPUT_DIR)/$(RPU_ELF_NAME).manifest

RPU_DEFAULT_LINKER_SCRIPT := $(SDK_FULL_PATH)/firmware/linker_scripts/zynqmp_r5.ld
RPU_USE_DEFAULT_LINKER_SCRIPT ?= 1
ifeq ($(strip $(RPU_USE_DEFAULT_LINKER_SCRIPT)),1)
RPU_LINKER_SCRIPT ?= $(RPU_OUTPUT_DIR)/$(notdir $(RPU_DEFAULT_LINKER_SCRIPT))
RPU_LINKER_SCRIPT_SRC ?= $(RPU_DEFAULT_LINKER_SCRIPT)
endif

RPU_USE_DEFAULT_STUBS ?= 1
ifeq ($(strip $(RPU_USE_DEFAULT_STUBS)),1)
RPU_SUPPORT_SRCS := $(RPU_OUTPUT_DIR)/support/newlib_stubs.c
RPU_SUPPORT_SRC ?= $(SDK_FULL_PATH)/firmware/newlib_stubs.c
$(RPU_SUPPORT_SRCS): $(RPU_SUPPORT_SRC) | $(RPU_OUTPUT_DIR)/
	@mkdir -p $(@D)
	cp $< $@
else
RPU_SUPPORT_SRCS :=
endif

# Expand a path relative to the project directory unless already absolute.
define __rpu_expand_path
$(if $(filter /%,$(1)),$(1),$(if $(filter $(TMP)/%,$(1)),$(1),$(PROJECT_PATH)/$(1)))
endef

# Collect full source file paths per language
RPU_ALL_C_SRCS    := $(RPU_SRCS) $(RPU_SUPPORT_SRCS)
RPU_C_SRC_FILES   := $(foreach f,$(RPU_ALL_C_SRCS),$(call __rpu_expand_path,$(f)))
RPU_CPP_SRC_FILES := $(foreach f,$(RPU_CPP_SRCS),$(call __rpu_expand_path,$(f)))
RPU_ASM_SRC_FILES := $(foreach f,$(RPU_ASM_SRCS),$(call __rpu_expand_path,$(f)))

# Sanitise a string so it can be used in an object filename
empty :=
space := $(empty) $(empty)
define __rpu_sanitise
$(subst +,_,$(subst -,_,$(subst .,_,$(subst /,_,$(subst $(space),_,$(strip $(1)))))))
endef

# Map each source file to a unique object path inside the build directory
RPU_C_OBJS   := $(foreach f,$(RPU_C_SRC_FILES),$(RPU_OBJ_DIR)/$(call __rpu_sanitise,$(f)).o)
RPU_CPP_OBJS := $(foreach f,$(RPU_CPP_SRC_FILES),$(RPU_OBJ_DIR)/$(call __rpu_sanitise,$(f)).o)
RPU_ASM_OBJS := $(foreach f,$(RPU_ASM_SRC_FILES),$(RPU_OBJ_DIR)/$(call __rpu_sanitise,$(f)).o)
RPU_OBJECTS  := $(RPU_C_OBJS) $(RPU_CPP_OBJS) $(RPU_ASM_OBJS)
RPU_DEPS     := $(RPU_OBJECTS:.o=.d)

RPU_CC   ?= $(DOCKER) arm-none-eabi-gcc
RPU_CXX  ?= $(DOCKER) arm-none-eabi-g++
RPU_OBJCOPY ?= $(DOCKER) arm-none-eabi-objcopy

RPU_CPUFLAGS := -mcpu=cortex-r5 -mfpu=vfpv3-d16 -mfloat-abi=hard
RPU_COMMON_FLAGS := -ffunction-sections -fdata-sections -fno-common -MMD -MP
RPU_WARNING_FLAGS := -Wall -Wextra -Wshadow -Wdouble-promotion -Wpointer-arith
RPU_OPTFLAGS ?= -O2

RPU_INCLUDE_DIRS ?=
RPU_DEFINES ?=
RPU_CPPFLAGS := $(addprefix -I,$(RPU_INCLUDE_DIRS)) $(addprefix -D,$(RPU_DEFINES))
RPU_CPPFLAGS += $(RPU_CPUFLAGS)

RPU_CFLAGS ?=
RPU_CFLAGS += $(RPU_COMMON_FLAGS) $(RPU_WARNING_FLAGS) $(RPU_OPTFLAGS)

RPU_CXXFLAGS ?=
RPU_CXXFLAGS += $(RPU_COMMON_FLAGS) $(RPU_WARNING_FLAGS) $(RPU_OPTFLAGS)
RPU_CXXFLAGS += -fno-exceptions -fno-rtti

RPU_ASFLAGS ?=
RPU_ASFLAGS += $(RPU_COMMON_FLAGS)

RPU_LDFLAGS ?=
RPU_LDFLAGS += $(RPU_CPUFLAGS) -specs=nosys.specs -Wl,--gc-sections
ifdef RPU_LINKER_SCRIPT
RPU_LDFLAGS += -Wl,-T,$(RPU_LINKER_SCRIPT)
endif

ifeq ($(strip $(RPU_USE_DEFAULT_LINKER_SCRIPT)),1)
$(RPU_LINKER_SCRIPT): $(RPU_LINKER_SCRIPT_SRC) | $(RPU_OUTPUT_DIR)/
	@mkdir -p $(@D)
	cp $< $@
endif
RPU_LIBS ?= -Wl,--start-group -lc -lgcc -lm -Wl,--end-group

RPU_ELF := $(RPU_OUTPUT_DIR)/$(RPU_ELF_NAME)

# Ensure build directories exist
$(RPU_OUTPUT_DIR)/:
	mkdir -p $@
	$(call ok,$@)

# Helper templates expanded via $(eval)
define __rpu_obj_rule_c
$(RPU_OBJ_DIR)/$(call __rpu_sanitise,$(1)).o: $(1) | $(RPU_OUTPUT_DIR)/
	@mkdir -p $$(dir $$@)
	$(RPU_CC) $(RPU_CPPFLAGS) $(RPU_CFLAGS) -c -o $$@ $$<
endef

define __rpu_obj_rule_cpp
$(RPU_OBJ_DIR)/$(call __rpu_sanitise,$(1)).o: $(1) | $(RPU_OUTPUT_DIR)/
	@mkdir -p $$(dir $$@)
	$(RPU_CXX) $(RPU_CPPFLAGS) $(RPU_CXXFLAGS) -c -o $$@ $$<
endef

define __rpu_obj_rule_asm
$(RPU_OBJ_DIR)/$(call __rpu_sanitise,$(1)).o: $(1) | $(RPU_OUTPUT_DIR)/
	@mkdir -p $$(dir $$@)
	$(RPU_CC) $(RPU_CPPFLAGS) $(RPU_ASFLAGS) -c -o $$@ $$<
endef

# Pattern rules for each source file type
$(foreach src,$(RPU_C_SRC_FILES),$(eval $(call __rpu_obj_rule_c,$(src))))
$(foreach src,$(RPU_CPP_SRC_FILES),$(eval $(call __rpu_obj_rule_cpp,$(src))))
$(foreach src,$(RPU_ASM_SRC_FILES),$(eval $(call __rpu_obj_rule_asm,$(src))))

-include $(RPU_DEPS)

$(RPU_ELF): $(RPU_OBJECTS) $(RPU_LINKER_SCRIPT) | $(RPU_OUTPUT_DIR)/
	$(RPU_CC) -o $@ $(RPU_OBJECTS) $(RPU_LDFLAGS) $(RPU_LIBS)
	$(call ok,$@)

# Firmware manifest entry for install step
$(RPU_ENTRY_FILE): $(RPU_ELF)
	@mkdir -p $(@D)
	printf '%s %s\n' '$(notdir $(RPU_ELF))' '$(RPU_INSTALL_PATH)' > $@

ifneq ($(strip $(RPU_REMOTEPROC)),)
RPU_REMOTEPROC_ENTRY := $(RPU_OUTPUT_DIR)/$(RPU_ELF_NAME).remoteproc

$(RPU_REMOTEPROC_ENTRY): $(RPU_ELF) | $(RPU_OUTPUT_DIR)/
	printf '%s %s\n' '$(RPU_REMOTEPROC)' '$(RPU_INSTALL_PATH)' > $@

REMOTEPROC_ENTRY_FILES += $(RPU_REMOTEPROC_ENTRY)
endif

FIRMWARE_TARGETS += $(RPU_ELF)
FIRMWARE_ENTRY_FILES += $(RPU_ENTRY_FILE)
INSTRUMENT_ADDITIONAL_FILES += $(RPU_ELF)

endif
