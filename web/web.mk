# Typescript compiler
###############################################################################

WEB_BUILD_MK := $(lastword $(MAKEFILE_LIST))
include $(dir $(WEB_BUILD_MK))../make/dependencies.mk
include $(WEB_PATH)/compiler-inputs.mk

###############################################################################
# Build webpage
###############################################################################

TMP_WEB_PATH := $(TMP_PROJECT_PATH)/web

WEB_DOWNLOADS_MK ?= $(WEB_PATH)/downloads.mk
include $(WEB_DOWNLOADS_MK)

WEB_FILES_ABS     := $(abspath $(WEB_FILES))
TS_FILES_ABS      := $(filter %.ts,$(WEB_FILES_ABS))
NON_TS_FILES_ABS  := $(filter-out %.ts,$(WEB_FILES_ABS))

TMP_WEB_PATH := $(TMP_PROJECT_PATH)/web

# A config/component edit can replace sources while keeping output basenames.
# Track make inputs as well as source mtimes so older shared assets replace
# newer legacy outputs and removed TypeScript files leave the compiled bundle.
WEB_CONFIG_FILES := $(filter %.mk Makefile,$(MAKEFILE_LIST))
WEB_SOURCE_SETTINGS := $(TMP_WEB_PATH)/.source-settings
$(eval $(call build_settings_stamp,$(WEB_SOURCE_SETTINGS),$$(foreach F,$$(TS_FILES_ABS),$$(call build_shell_quote,$$(F)))))
WEB_ASSET_SETTINGS := $(TMP_WEB_PATH)/.asset-settings
$(eval $(call build_settings_stamp,$(WEB_ASSET_SETTINGS),$$(foreach F,$$(NON_TS_FILES_ABS),$$(call build_shell_quote,$$(F)))))

ifeq ($(TS_FILES_ABS),)
  APP_JS :=
else
  APP_JS := $(TMP_WEB_PATH)/app.js
$(APP_JS): $(TS_FILES_ABS) $(WEB_CONFIG_FILES) $(WEB_COMPILER_INPUTS) $(WEB_SOURCE_SETTINGS) | $(TMP_WEB_PATH)/
	mkdir -p $(@D)
	$(WEB_COMPILE) --output "$@" $(TS_FILES_ABS)
endif

BASENAMES            := $(notdir $(NON_TS_FILES_ABS))
DUPLICATE_BASENAMES  := $(strip $(foreach name,$(sort $(BASENAMES)),$(if $(word 2,$(filter $(name),$(BASENAMES))),$(name))))
ifneq ($(DUPLICATE_BASENAMES),)
  $(error Duplicate web asset basename(s): $(DUPLICATE_BASENAMES))
endif

FLAT_ASSET_TARGETS   := $(addprefix $(TMP_WEB_PATH)/,$(BASENAMES))

define COPY_ONE
$(TMP_WEB_PATH)/$(notdir $1): $1 $(WEB_CONFIG_FILES) $(WEB_ASSET_SETTINGS) | $(TMP_WEB_PATH)/
	cp $$< $$@
endef
$(foreach f,$(NON_TS_FILES_ABS),$(eval $(call COPY_ONE,$(f))))

WEB_ASSETS := $(WEB_DOWNLOADS) $(APP_JS) $(FLAT_ASSET_TARGETS)

.PHONY: web
web: $(WEB_ASSETS)

.PHONY: clean_web
clean_web:
	rm -rf $(TMP_WEB_PATH)
