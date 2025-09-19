
# Docker builder
###############################################################################

WEB_BUILDER_IMG ?= koheron-web:node20

.PHONY: web_builder
web_builder:
	docker build -f $(WEB_PATH)/Dockerfile.web -t $(WEB_BUILDER_IMG) $(WEB_PATH)

WEB_DOCKER_RUN := docker run --rm -t \
                  -u $$(id -u):$$(id -g) \
                  -v $(PWD):/work \
                  -w /work \
                  $(WEB_BUILDER_IMG)

# Typescript compiler
###############################################################################

TSC_FLAGS ?= -pretty \
             --target ES5 \
             --lib es6,dom \
             --alwaysStrict \
             --skipLibCheck \
             --module system \
             --incremental
TSC       ?= $(WEB_DOCKER_RUN) tsc $(TSC_FLAGS)

# Build webpage
###############################################################################

TMP_WEB_PATH := $(TMP_PROJECT_PATH)/web

WEB_DOWNLOADS_MK ?= $(WEB_PATH)/downloads.mk
include $(WEB_DOWNLOADS_MK)

WEB_FILES := $(shell $(MAKE_PY) --web $(CONFIG_YML) $(TMP_WEB_PATH)/web_files && cat $(TMP_WEB_PATH)/web_files)

TS_FILES      := $(filter %.ts,$(WEB_FILES))
NON_TS_FILES  := $(filter-out %.ts,$(WEB_FILES))

# Paths relative to ".../web/"
REL_NON_TS := $(patsubst %/web/%,%,$(NON_TS_FILES))
# Where to find sources (no SDK/PROJECT in rules)
VPATH := $(sort $(patsubst %/web/%,%/web,$(NON_TS_FILES)))

# Targets (preserve subdirectories)
TMP_NON_TS_FILES := $(addprefix $(TMP_WEB_PATH)/,$(REL_NON_TS))

# Typescript → single bundle
ifeq ($(TS_FILES),)
  APP_JS :=
else
  APP_JS := $(TMP_WEB_PATH)/app.js
$(APP_JS): $(TS_FILES) | $(TMP_WEB_PATH)/
	mkdir -p $(@D)
	$(TSC) $^ --outFile $@
endif

# Generic copy rule for non-TS assets
$(TMP_WEB_PATH)/%: % | $(TMP_WEB_PATH)/
	mkdir -p $(@D)
	cp $< $@

# Ensure the staging root exists (prevents -j races)
$(TMP_WEB_PATH)/:
	mkdir -p $@

WEB_ASSETS := $(WEB_DOWNLOADS) $(APP_JS) $(TMP_NON_TS_FILES)

.PHONY: web
web: $(WEB_ASSETS)

.PHONY: clean_web
clean_web:
	rm -rf $(TMP_WEB_PATH)