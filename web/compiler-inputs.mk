ifndef KOHERON_WEB_COMPILER_INCLUDED
KOHERON_WEB_COMPILER_INCLUDED := 1

# The dashboard includes this before web.mk, so define the compiler here too.
WEB_COMPILER_INPUTS := $(WEB_PATH)/compiler-inputs.mk $(WEB_PATH)/package.json $(WEB_PATH)/package-lock.json $(WEB_PATH)/Dockerfile.web $(WEB_PATH)/build.cjs $(WEB_PATH)/transpile.cjs

ifeq ($(origin WEB_COMPILE),undefined)
WEB_DOCKER_IMAGE ?= koheron-web:node24
CURRENT_DIR := $(shell pwd -P)
WEB_DOCKER_RUN ?= docker run --rm \
                  -u $$(id -u):$$(id -g) \
                  -v $(CURRENT_DIR):$(CURRENT_DIR) \
                  -w $(CURRENT_DIR) $(WEB_DOCKER_IMAGE)
WEB_COMPILE = $(WEB_DOCKER_RUN) node /opt/app/build.cjs

WEB_BUILDER_STAMP := $(if $(TMP),$(TMP),$(TMP_PROJECT_PATH))/web/.compiler-settings
WEB_COMPILER_INPUTS += $(WEB_PATH)/ensure_builder.sh $(WEB_PATH)/plotting/package.json $(WEB_PATH)/plotting/package-lock.json
.PHONY: web-builder-force
web-builder-force:

$(WEB_BUILDER_STAMP): web-builder-force $(WEB_COMPILER_INPUTS)
	bash $(WEB_PATH)/ensure_builder.sh "$(WEB_DOCKER_IMAGE)" "$@" "$(abspath $(WEB_PATH))"

WEB_COMPILER_INPUTS += $(WEB_BUILDER_STAMP)
endif
endif
