
# Docker builder
###############################################################################

WEB_BUILDER_IMG ?= koheron-web:node20

.PHONY: web_builder
web_builder:
	docker build -f $(WEB_PATH)/Dockerfile.web -t $(WEB_BUILDER_IMG) $(WEB_PATH)

WEB_DOCKER_RUN := docker run --rm -t \
                  -u $$(id -u):$$(id -g) \
                  -v $(PWD):$(PWD) \
                  -w $(PWD) \
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

WEB_FILES_ABS := $(abspath $(WEB_FILES))

TS_FILES_ABS     := $(filter %.ts,$(WEB_FILES_ABS))
NON_TS_FILES_ABS := $(filter-out %.ts,$(WEB_FILES_ABS))

TMP_WEB_PATH := $(TMP_PROJECT_PATH)/web

ifeq ($(TS_FILES_ABS),)
  APP_JS :=
else
  APP_JS := $(TMP_WEB_PATH)/app.js
$(APP_JS): $(TS_FILES_ABS) | $(TMP_WEB_PATH)/
	mkdir -p $(@D)
	$(TSC) $^ --outFile $@
endif

ASSETS_STAMP := $(TMP_WEB_PATH)/.assets.stamp

# Absolute roots for robust stripping
SDK_ROOT      := $(abspath $(SDK_PATH))
PROJECT_ROOT  := $(abspath $(PROJECT_PATH))

$(ASSETS_STAMP): $(NON_TS_FILES_ABS) | $(TMP_WEB_PATH)/
	@set -e; \
	for src in $^; do \
	  rel="$${src#$(SDK_ROOT)/}"; \
	  rel="$${rel#$(PROJECT_ROOT)/}"; \
	  case "$$rel" in \
	    web/*) : ;; \
	    */web/*) rel="$${rel#*web/}"; rel="web/$$rel" ;; \
	    *) rel="$$(basename "$$rel")" ;; \
	  esac; \
	  dst="$(TMP_WEB_PATH)/$$rel"; \
	  mkdir -p "$$(dirname "$$dst")"; \
	  cp "$$src" "$$dst"; \
	done; \
	touch $@

# Ensure the staging root exists
$(TMP_WEB_PATH)/:
	mkdir -p $@

WEB_ASSETS := $(WEB_DOWNLOADS) $(APP_JS) $(ASSETS_STAMP)

.PHONY: web
web: $(WEB_ASSETS)

.PHONY: clean_web
clean_web:
	rm -rf $(TMP_WEB_PATH)
