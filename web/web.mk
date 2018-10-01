TMP_WEB_PATH := $(TMP_PROJECT_PATH)/web

WEB_DOWNLOADS_MK ?= $(WEB_PATH)/downloads.mk
include $(WEB_DOWNLOADS_MK)

# Typescript compiler
TSC_BIN := node_modules/typescript/bin/tsc
TSC ?= $(TSC_BIN) --pretty --target ES3 --lib es6,dom --alwaysStrict

WEB_FILES := $(shell $(MAKE_PY) --web $(CONFIG) $(TMP_WEB_PATH)/web_files && cat $(TMP_WEB_PATH)/web_files)

TS_FILES := $(filter %.ts,$(WEB_FILES))
NO_TS_FILES := $(filter-out $(TS_FILES),$(WEB_FILES))
TMP_NO_TS_FILES := $(addprefix $(TMP_WEB_PATH)/, $(notdir $(NO_TS_FILES)))

ifeq ($(TS_FILES),)
APP_JS :=
else
APP_JS := $(TMP_WEB_PATH)/app.js
$(APP_JS): $(TS_FILES)
	mkdir -p $(@D)
	$(TSC) $^ --outFile $@
endif

define copy_no_ts_file
$(TMP_WEB_PATH)/$(notdir $1): $1
	cp $$< $$@
endef
$(foreach file,$(NO_TS_FILES),$(eval $(call copy_no_ts_file,$(file))))

WEB_ASSETS := $(WEB_DOWNLOADS) $(APP_JS) $(TMP_NO_TS_FILES)

.PHONY: web
web: $(WEB_ASSETS)

# Clean targets
###############################################################################

.PHONY: clean_web
clean_web:
	rm -rf $(TMP_WEB_PATH)