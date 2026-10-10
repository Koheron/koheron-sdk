ifndef KOHERON_BUILD_DEPENDENCIES_INCLUDED
KOHERON_BUILD_DEPENDENCIES_INCLUDED := 1
BUILD_DEPENDENCIES_MK := $(lastword $(MAKEFILE_LIST))

# make.py imports the server generator even when only rendering a memory map.
BUILD_SDK_PATH := $(if $(SDK_PATH),$(SDK_PATH),$(abspath $(dir $(BUILD_DEPENDENCIES_MK))..))
MAKE_PY_SOURCES := $(BUILD_SDK_PATH)/make.py $(BUILD_SDK_PATH)/server/__init__.py

build_shell_quote = '$(subst ','"'"',$(1))'

.PHONY: build-settings-force
build-settings-force:

# Publish settings atomically, preserving the timestamp on unchanged builds.
# Pass shell-quoted printf arguments as $(2); defer variables with $$ in eval.
define build_settings_stamp
$(1): build-settings-force $(BUILD_DEPENDENCIES_MK)
	@mkdir -p "$$(@D)"
	@stamp_tmp=$$$$(mktemp "$$@.tmp.XXXXXX"); \
	trap 'rm -f "$$$$stamp_tmp"' EXIT; \
	trap 'exit 129' HUP; trap 'exit 130' INT; trap 'exit 143' TERM; \
	printf '%s\n' $(2) > "$$$$stamp_tmp"; \
	if ! cmp -s "$$$$stamp_tmp" "$$@"; then mv -f "$$$$stamp_tmp" "$$@"; fi
endef
endif
