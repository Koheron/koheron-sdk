ifndef KOHERON_COMPILER_SETTINGS_INCLUDED
KOHERON_COMPILER_SETTINGS_INCLUDED := 1
COMPILER_SETTINGS_MK := $(lastword $(MAKEFILE_LIST))
HOST_GCC_VERSION ?= $(if $(GCC_VERSION),$(GCC_VERSION),13)
KERNEL_GCC_VERSION ?= $(if $(GCC_VERSION),$(GCC_VERSION),13)
UBOOT_GCC_VERSION ?= $(if $(GCC_VERSION),$(GCC_VERSION),13)
ATF_GCC_VERSION ?= $(if $(GCC_VERSION),$(GCC_VERSION),13)
OS_HOSTCC = gcc-$(HOST_GCC_VERSION)
OS_HOSTCXX = g++-$(HOST_GCC_VERSION)
KERNEL_CC = $(GCC_ARCH)-gcc-$(KERNEL_GCC_VERSION)
UBOOT_CC = $(GCC_ARCH)-gcc-$(UBOOT_GCC_VERSION)
ATF_CC = $(GCC_ARCH)-gcc-$(ATF_GCC_VERSION)

KERNEL_TOOLCHAIN_FLAGS = CROSS_COMPILE=$(GCC_ARCH)- CC=$(KERNEL_CC) HOSTCC=$(OS_HOSTCC) HOSTCXX=$(OS_HOSTCXX) AR=$(GCC_ARCH)-gcc-ar-$(KERNEL_GCC_VERSION) NM=$(GCC_ARCH)-gcc-nm-$(KERNEL_GCC_VERSION) RANLIB=$(GCC_ARCH)-gcc-ranlib-$(KERNEL_GCC_VERSION)
UBOOT_TOOLCHAIN_FLAGS = CROSS_COMPILE=$(GCC_ARCH)- CC=$(UBOOT_CC) HOSTCC=$(OS_HOSTCC) HOSTCXX=$(OS_HOSTCXX) AR=$(GCC_ARCH)-gcc-ar-$(UBOOT_GCC_VERSION) NM=$(GCC_ARCH)-gcc-nm-$(UBOOT_GCC_VERSION) RANLIB=$(GCC_ARCH)-gcc-ranlib-$(UBOOT_GCC_VERSION)

.PHONY: compiler-settings-force
compiler-settings-force:

# Reconfigure/rebuild cached OS output when the selected compiler or image changes.
define compiler_settings_stamp
$(1): compiler-settings-force $(COMPILER_SETTINGS_MK)
	@mkdir -p "$$(@D)"
	@stamp_tmp=$$$$(mktemp "$$@.tmp.XXXXXX"); \
	trap 'rm -f "$$$$stamp_tmp"' EXIT; \
	trap 'exit 129' HUP; trap 'exit 130' INT; trap 'exit 143' TERM; \
	{ printf '%s\n' 'CC=$(2)' 'HOSTCC=$(OS_HOSTCC)' 'HOSTCXX=$(OS_HOSTCXX)' 'IMAGE=$(DOCKER_IMAGE)'; \
	  $(if $(strip $(DOCKER_IMAGE)),docker image inspect --format '{{.Id}}' '$(DOCKER_IMAGE)',:); \
	} > "$$$$stamp_tmp"; \
	if ! cmp -s "$$$$stamp_tmp" "$$@"; then mv -f "$$$$stamp_tmp" "$$@"; fi
endef
endif
