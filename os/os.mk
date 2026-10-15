ABS_TMP_OS_PATH := $(abspath $(TMP_OS_PATH))

TMP_OS_BOARD_PATH ?= $(TMP_PROJECT_PATH)/os
LINUX_SELECTION_STAMP := $(TMP_OS_PATH)/.linux-selection

include $(dir $(lastword $(MAKEFILE_LIST)))compiler-settings.mk
include $(dir $(lastword $(MAKEFILE_LIST)))../make/dependencies.mk
# U-Boot can be upgraded independently of the FSBL and FPGA toolchain.
UBOOT_VERSION ?= 2026.1
UBOOT_TAG := xilinx-uboot-v$(UBOOT_VERSION)
DTREE_TAG ?= xilinx_v$(VIVADO_VERSION)

UBOOT_URL := https://github.com/Xilinx/u-boot-xlnx/archive/refs/tags/xilinx-v$(UBOOT_VERSION).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/refs/tags/$(DTREE_TAG).tar.gz

UBOOT_PATH ?= $(TMP_OS_BOARD_PATH)/u-boot-xlnx-$(UBOOT_TAG)
DTREE_PATH := $(TMP_OS_PATH)/device-tree-xlnx-$(DTREE_TAG)

UBOOT_TAR := $(TMP)/u-boot-xlnx-$(UBOOT_TAG).tar.gz
DTREE_TAR := $(TMP)/device-tree-xlnx-$(DTREE_TAG).tar.gz

UBOOT_PATCH_FILES := $(sort $(shell test -d $(PATCHES)/u-boot && find $(PATCHES)/u-boot -type f))
UBOOT_CONFIG_FILE := $(wildcard $(PATCHES)/$(UBOOT_CONFIG))
UBOOT_PATCH_SELECTION_STAMP := $(TMP_OS_BOARD_PATH)/.uboot-patches
$(eval $(call build_settings_stamp,$(UBOOT_PATCH_SELECTION_STAMP),$$(foreach F,$$(UBOOT_PATCH_FILES) $$(UBOOT_CONFIG_FILE),$$(call build_shell_quote,$$(F)))))

BOOT_MEDIUM ?= mmcblk0

###############################################################################
# fsbl/executable.elf
###############################################################################

# Additional files (including fsbl_hooks.c) can be added to the FSBL in $(BOARD_PATH)/patches/fsbl
ifndef FSBL_PATH
FSBL_PATH := $(BOARD_PATH)/patches/fsbl
endif
FSBL_FILES := $(wildcard $(FSBL_PATH)/*.h $(FSBL_PATH)/*.c $(FSBL_PATH)/*.py)

.PHONY: fsbl
fsbl: $(TMP_OS_PATH)/fsbl/executable.elf

$(TMP_OS_PATH)/hard/$(NAME).xsa: $(TMP_FPGA_PATH)/$(NAME).xsa | $(TMP_OS_PATH)/hard/
	cp $< $@
	$(call ok,$@)

FSBL_SETTINGS_STAMP := $(TMP_OS_PATH)/.fsbl-settings
$(eval $(call build_settings_stamp,$(FSBL_SETTINGS_STAMP),$$(call build_shell_quote,STDIO=$$(FSBL_STDIO))))

$(TMP_OS_PATH)/fsbl/Makefile:  $(TMP_OS_PATH)/hard/$(NAME).xsa $(FPGA_PATH)/hsi/fsbl.tcl $(FSBL_SETTINGS_STAMP) | $(TMP_OS_PATH)/fsbl/
	$(HSI) $(FPGA_PATH)/hsi/fsbl.tcl $(NAME) $(PROC) $(TMP_OS_PATH)/hard $(@D) $< $(ZYNQ_TYPE) $(FSBL_STDIO)
	$(call ok,$@)

$(TMP_OS_PATH)/fsbl/executable.elf: $(TMP_OS_PATH)/fsbl/Makefile $(FSBL_FILES)
	cp -a $(FSBL_PATH)/. $(TMP_OS_PATH)/fsbl/ 2>/dev/null || true
	@if test -f $(FSBL_PATH)/apply_ps7_init_patch.py; then \
		echo "Patching ps7_init.c ..."; \
		python3 $(FSBL_PATH)/apply_ps7_init_patch.py $(TMP_OS_PATH)/fsbl; \
	fi
	source $(VIVADO_PATH)/settings64.sh && $(DOCKER) make -C $(@D) all
	$(call ok,$@)

.PHONY: clean_fsbl
clean_fsbl:
	rm -rf $(TMP_OS_PATH)/fsbl

###############################################################################
# u-boot.elf
###############################################################################

.PHONY: uboot
uboot: $(TMP_OS_BOARD_PATH)/u-boot.elf

$(UBOOT_TAR): $(SOURCE_CHECKSUMS) $(DOWNLOAD_VERIFIED)
	mkdir -p $(@D)
	bash $(DOWNLOAD_VERIFIED) $(SOURCE_CHECKSUMS) $@ $(UBOOT_URL)
	$(call ok,$@)

$(UBOOT_PATH)/.unpacked: $(UBOOT_TAR) $(SOURCE_CHECKSUMS) $(DOWNLOAD_VERIFIED) $(UBOOT_PATCH_SELECTION_STAMP) | $(UBOOT_PATH)/
	bash $(DOWNLOAD_VERIFIED) $(SOURCE_CHECKSUMS) $(UBOOT_TAR) $(UBOOT_URL)
	rm -rf -- "$(@D)"
	mkdir -p -- "$(@D)"
	tar -zxf $< --strip-components=1 -C $(@D)
	touch $@
	$(call ok,$@)

# Configure U-Boot once to avoid concurrent defconfig/mrproper races
UBOOT_CONFIG_STAMP := $(UBOOT_PATH)/.config
UBOOT_COMPILER_STAMP := $(UBOOT_PATH)/.compiler-settings
$(eval $(call compiler_settings_stamp,$(UBOOT_COMPILER_STAMP),$(UBOOT_CC)))
$(UBOOT_COMPILER_STAMP): $(UBOOT_PATH)/.unpacked

# Switching back to a cached release must also replace the shared output ELF.
UBOOT_SELECTION_STAMP := $(TMP_OS_BOARD_PATH)/.uboot-selection
$(UBOOT_SELECTION_STAMP): compiler-settings-force
	@mkdir -p "$(@D)"
	@selection_tmp=$$(mktemp "$@.tmp.XXXXXX"); \
	trap 'rm -f "$$selection_tmp"' EXIT; \
	printf '%s\n' 'TAG=$(UBOOT_TAG)' 'PATH=$(UBOOT_PATH)' 'CONFIG=$(UBOOT_CONFIG)' 'FRAGMENTS=$(UBOOT_CONFIG_FRAGMENTS)' > "$$selection_tmp"; \
	if ! cmp -s "$$selection_tmp" "$@"; then mv -f "$$selection_tmp" "$@"; fi

$(UBOOT_CONFIG_STAMP): $(UBOOT_PATH)/.unpacked $(UBOOT_PATCH_FILES) $(UBOOT_CONFIG_FILE) $(UBOOT_CONFIG_FRAGMENTS) $(UBOOT_COMPILER_STAMP) $(UBOOT_SELECTION_STAMP) $(OS_PATH)/scripts/patch_uboot.sh
	cp -a $(PATCHES)/${UBOOT_CONFIG} $(UBOOT_PATH)/ 2>/dev/null || true
	cp -a $(PATCHES)/u-boot/. $(UBOOT_PATH)/ 2>/dev/null || true
	bash $(OS_PATH)/scripts/patch_uboot.sh $(UBOOT_PATH)
	$(DOCKER) make -C $(UBOOT_PATH) mrproper
	$(DOCKER) make -C $(UBOOT_PATH) ARCH=$(UBOOT_ARCH) $(UBOOT_TOOLCHAIN_FLAGS) $(UBOOT_CONFIG)
ifneq ($(strip $(UBOOT_CONFIG_FRAGMENTS)),)
	cd $(UBOOT_PATH) && scripts/kconfig/merge_config.sh -m .config $(abspath $(UBOOT_CONFIG_FRAGMENTS))
	$(DOCKER) make -C $(UBOOT_PATH) ARCH=$(UBOOT_ARCH) $(UBOOT_TOOLCHAIN_FLAGS) olddefconfig
endif
	@touch $@
	$(call ok,$@)

$(TMP_OS_BOARD_PATH)/u-boot.elf: $(UBOOT_CONFIG_STAMP) $(UBOOT_SELECTION_STAMP) | $(TMP_BOARD_PATH)/
	$(DOCKER) make -C $(UBOOT_PATH) --jobs=$(N_CPUS) ARCH=$(UBOOT_ARCH) CFLAGS="$(UBOOT_CFLAGS) $(GCC_FLAGS)" \
	  $(UBOOT_TOOLCHAIN_FLAGS) all
	if [ -f $(UBOOT_PATH)/u-boot.elf ]; then cp $(UBOOT_PATH)/u-boot.elf $@; else cp $(UBOOT_PATH)/u-boot $@; fi
	$(call ok,$@)

$(TMP_OS_PATH)/u-boot.elf: $(TMP_OS_BOARD_PATH)/u-boot.elf | $(TMP_OS_PATH)/
	cp $< $@
	$(call ok,$@)

###############################################################################
# pmu/executable.elf
###############################################################################

.PHONY: pmufw
pmufw: $(TMP_OS_PATH)/pmu/executable.elf

PMU_STDIO ?=
PMU_CFLAGS ?=
PMU_SETTINGS_STAMP := $(TMP_OS_PATH)/.pmu-settings
$(eval $(call build_settings_stamp,$(PMU_SETTINGS_STAMP),$$(call build_shell_quote,STDIO=$$(PMU_STDIO)) $$(call build_shell_quote,CFLAGS=$$(PMU_CFLAGS))))

$(TMP_OS_PATH)/pmu/Makefile: $(TMP_OS_PATH)/hard/$(NAME).xsa $(FPGA_PATH)/hsi/pmufw.tcl $(PMU_SETTINGS_STAMP)
	rm -rf -- "$(@D)"
	mkdir -p "$(@D)"
	$(HSI) $(FPGA_PATH)/hsi/pmufw.tcl $(NAME) $(TMP_OS_PATH)/hard $(@D) $< "$(PMU_STDIO)"
	$(call ok,$@)

$(TMP_OS_PATH)/pmu/executable.elf: $(TMP_OS_PATH)/pmu/Makefile
	# Vendor BSP recipes run clean alongside compilation under -j.
	source $(VITIS_PATH)/settings64.sh && \
	  PATH="$(VITIS_PATH)/gnu/microblaze/lin/bin:$$PATH" $(MAKE) -j1 -C $(@D) CFLAGS="$(PMU_CFLAGS)" all

.PHONY: clean_pmufw
clean_pmufw:
	rm -rf $(TMP_OS_PATH)/pmu

###############################################################################
# bl31.elf
###############################################################################

$(ATRUST_TAR): $(SOURCE_CHECKSUMS) $(DOWNLOAD_VERIFIED)
	mkdir -p $(@D)
	bash $(DOWNLOAD_VERIFIED) $(SOURCE_CHECKSUMS) $@ $(ARMTRUST_URL)
	$(call ok,$@)

$(ATRUST_PATH)/.unpacked: $(ATRUST_TAR) $(SOURCE_CHECKSUMS) $(DOWNLOAD_VERIFIED) | $(ATRUST_PATH)/
	bash $(DOWNLOAD_VERIFIED) $(SOURCE_CHECKSUMS) $(ATRUST_TAR) $(ARMTRUST_URL)
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	$(call ok,$@)

ATF_COMPILER_STAMP := $(ATRUST_PATH)/.compiler-settings
$(eval $(call compiler_settings_stamp,$(ATF_COMPILER_STAMP),$(ATF_CC)))
ATF_CONSOLE ?= cadence
ATF_CONSOLE_STAMP := $(ATRUST_PATH)/.console-settings
$(eval $(call build_settings_stamp,$(ATF_CONSOLE_STAMP),$$(call build_shell_quote,CONSOLE=$$(ATF_CONSOLE))))
$(ATF_CONSOLE_STAMP): $(ATRUST_PATH)/.unpacked
$(TMP_OS_PATH)/bl31.elf: $(ATRUST_PATH)/.unpacked $(ATF_COMPILER_STAMP) $(ATF_CONSOLE_STAMP)
	# TF-A does not track compiler command changes in its cached objects.
	$(DOCKER) make PLAT=zynqmp clean -C $(ATRUST_PATH)
	$(DOCKER) make CROSS_COMPILE=$(GCC_ARCH)- CC=$(ATF_CC) PLAT=zynqmp bl31 ZYNQMP_CONSOLE=$(ATF_CONSOLE) ZYNQMP_ATF_MEM_BASE=0x10000 ZYNQMP_ATF_MEM_SIZE=0x40000 -C $(ATRUST_PATH)
	cp $(ATRUST_PATH)/build/zynqmp/release/bl31/bl31.elf $@
	$(call ok,$@)

###############################################################################
# boot.bin
###############################################################################

$(TMP_OS_PATH)/boot.bin: \
  $(TMP_OS_PATH)/fsbl/executable.elf \
  $(BITSTREAM) \
  $(TMP_OS_PATH)/u-boot.elf
	echo "img:{[bootloader] $(TMP_OS_PATH)/fsbl/executable.elf" > $(TMP_OS_PATH)/boot.bif
	echo " $(BITSTREAM)" >> $(TMP_OS_PATH)/boot.bif
	echo " $(TMP_OS_PATH)/u-boot.elf" >> $(TMP_OS_PATH)/boot.bif
	echo " }" >> $(TMP_OS_PATH)/boot.bif
	$(BOOTGEN) -image $(TMP_OS_PATH)/boot.bif -arch $(ZYNQ_TYPE) -w -o i $@
	$(call ok,$@)

$(TMP_OS_PATH)/bootmp.bin: \
  $(TMP_OS_PATH)/pmu/executable.elf \
  $(TMP_OS_PATH)/bl31.elf \
  $(TMP_OS_PATH)/fsbl/executable.elf \
  $(BITSTREAM) \
  $(TMP_OS_PATH)/u-boot.elf
	echo "img:{" > $(TMP_OS_PATH)/boot.bif
	echo "[pmufw_image] $(TMP_OS_PATH)/pmu/executable.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "[bootloader,destination_cpu=a53-0] $(TMP_OS_PATH)/fsbl/executable.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "[destination_device=pl] $(BITSTREAM)" >> $(TMP_OS_PATH)/boot.bif
	echo "[destination_cpu=a53-0,exception_level=el-3] $(TMP_OS_PATH)/bl31.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "[destination_cpu=a53-0,exception_level=el-2] $(TMP_OS_PATH)/u-boot.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "}" >> $(TMP_OS_PATH)/boot.bif
	$(BOOTGEN) -image $(TMP_OS_PATH)/boot.bif -arch $(ZYNQ_TYPE) -w -o i $@
	cp $(TMP_OS_PATH)/bootmp.bin $(TMP_OS_PATH)/boot.bin
	$(call ok,$@)

###############################################################################
# devicetree.dtb
###############################################################################

$(DTREE_TAR): $(SOURCE_CHECKSUMS) $(DOWNLOAD_VERIFIED)
	mkdir -p $(@D)
	bash $(DOWNLOAD_VERIFIED) $(SOURCE_CHECKSUMS) $@ $(DTREE_URL)
	$(call ok,$@)

$(DTREE_PATH)/.unpacked: $(DTREE_TAR) $(SOURCE_CHECKSUMS) $(DOWNLOAD_VERIFIED) | $(DTREE_PATH)/
	bash $(DOWNLOAD_VERIFIED) $(SOURCE_CHECKSUMS) $(DTREE_TAR) $(DTREE_URL)
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	$(call ok,$@)

.PHONY: devicetree
devicetree: $(TMP_OS_PATH)/devicetree/system-top.dts

$(TMP_OS_PATH)/devicetree/system-top.dts: $(TMP_OS_PATH)/hard/$(NAME).xsa $(DTREE_PATH)/.unpacked
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/devicetree.tcl $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VERSION) $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/devicetree $< $(BOOT_MEDIUM)
	$(call ok,$@)

$(TMP_OS_PATH)/devicetree.dtb: $(DTC_BIN) $(TMP_OS_PATH)/devicetree/system-top.dts $(LINUX_SELECTION_STAMP)
	$(DOCKER) $(OS_HOSTCC) -I $(TMP_OS_PATH)/devicetree/ -I $(TMP_OS_PATH)/devicetree/include/ -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp -o \
		$(TMP_OS_PATH)/devicetree/system-top.dts.tmp $(TMP_OS_PATH)/devicetree/system-top.dts
	$(DOCKER) $(DTC_BIN) -I dts -O dtb -o $@ \
	  -i $(TMP_OS_PATH)/devicetree -b 0 -@ $(TMP_OS_PATH)/devicetree/system-top.dts.tmp
	$(call ok,$@)

.PHONY: clean_devicetree
clean_devicetree:
	rm -rf $(TMP_OS_PATH)/devicetree

###############################################################################
# pl.dtbo
###############################################################################

$(TMP_OS_PATH)/pl-overlay/pl.dtsi: $(TMP_OS_PATH)/hard/$(NAME).xsa $(DTREE_PATH)/.unpacked | $(TMP_OS_PATH)/pl-overlay/
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/devicetree.tcl $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VERSION) $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/pl-overlay $< $(BOOT_MEDIUM)
	$(call ok,$@)

$(TMP_OS_PATH)/pl-overlay/memory.dtsi: $(MEMORY_YML) $(FPGA_PATH)/memory.dtsi $(MAKE_PY_SOURCES) | $(TMP_OS_PATH)/pl-overlay/ $(PYTHON_REQUIREMENTS_STAMP)
	$(MAKE_PY) --memory_dtsi $@ $<
	$(call ok,$@)

OVERRIDE_DTSI ?= $(FPGA_PATH)/override.dtsi
OVERRIDE_DTSI_SELECTION_STAMP := $(TMP_OS_PATH)/pl-overlay/.override-selection
$(eval $(call build_settings_stamp,$(OVERRIDE_DTSI_SELECTION_STAMP),$$(call build_shell_quote,$$(abspath $$(OVERRIDE_DTSI)))))

$(TMP_OS_PATH)/pl-overlay/override.dtsi: $(OVERRIDE_DTSI) $(OVERRIDE_DTSI_SELECTION_STAMP) | $(TMP_OS_PATH)/pl-overlay/
	cp $< $@
	$(call ok,$@)

$(TMP_OS_PATH)/pl-overlay/pl_wrap.dts: $(FPGA_PATH)/pl_wrap.dts | $(TMP_OS_PATH)/pl-overlay/
	sed -E 's|/include/[[:space:]]+"pl\.dtsi"|/include/ "pl-koheron.dtsi"|' $< > $@
	$(call ok,$@)

$(TMP_OS_PATH)/pl-overlay/pl-koheron.dtsi: $(TMP_OS_PATH)/pl-overlay/pl.dtsi FORCE
	@sed '/^[[:space:]]*firmware-name[[:space:]]*=/d' $< > $@.tmp
	@cmp -s $@.tmp $@ || mv -f $@.tmp $@
	@rm -f $@.tmp

$(TMP_OS_PATH)/pl.dtbo: $(DTC_BIN) $(LINUX_SELECTION_STAMP) \
  $(OS_PATH)/os.mk \
  $(TMP_OS_PATH)/pl-overlay/pl-koheron.dtsi \
  $(TMP_OS_PATH)/pl-overlay/memory.dtsi \
  $(TMP_OS_PATH)/pl-overlay/override.dtsi \
  $(TMP_OS_PATH)/pl-overlay/pl_wrap.dts
	# Runtime overlays are not targets for stacked overlays. Keep their fixups,
	# but do not export symbols onto the permanent base-tree /__symbols__ node.
	$(DOCKER) $(DTC_BIN) -I dts -O dtb -b 0 \
	  -i $(TMP_OS_PATH)/pl-overlay \
	  -o $@ $(TMP_OS_PATH)/pl-overlay/pl_wrap.dts
	$(call ok,$@)

$(TMP_PROJECT_PATH)/pl.dtbo: $(TMP_OS_PATH)/pl.dtbo
	cp $(TMP_OS_PATH)/pl.dtbo  $(TMP_PROJECT_PATH)/pl.dtbo

###############################################################################
# board.dtbo
###############################################################################

BOARD_DTSO ?= $(OS_PATH)/board.dtso

# Outputs below are shared by all cached kernel releases for this instrument.
$(eval $(call build_settings_stamp,$(LINUX_SELECTION_STAMP),\
    $$(call build_shell_quote,TAG=$$(LINUX_TAG)) \
    $$(call build_shell_quote,PATH=$$(LINUX_PATH)) \
    $$(call build_shell_quote,ARCH=$$(ARCH)) \
    $$(call build_shell_quote,KERNEL=$$(KERNEL_BIN)) \
    $$(call build_shell_quote,OVERLAY=$$(BOARD_DTSO))))

$(TMP_OS_PATH)/board-overlay/board.dtso: $(BOARD_DTSO) $(BOARD_DTSO_DEPS) $(LINUX_SELECTION_STAMP) | $(TMP_OS_PATH)/board-overlay/
	cp $< $@
	$(call ok,$@)

# Some boards select an overlay shipped inside the kernel archive. Make must
# unpack it before resolving that source, and restage it after a new unpack.
ifneq ($(filter $(LINUX_PATH)/%,$(BOARD_DTSO)),)
$(BOARD_DTSO): | $(LINUX_PATH)/.unpacked
	@test -f "$@" || { echo "Missing board overlay after kernel unpack: $@" >&2; exit 1; }

$(TMP_OS_PATH)/board-overlay/board.dtso: $(LINUX_PATH)/.unpacked
endif

$(TMP_OS_PATH)/board-overlay/board.dtbo: $(TMP_OS_PATH)/board-overlay/board.dtso $(DTC_BIN) $(LINUX_SELECTION_STAMP)
	# Preprocess so #include <dt-bindings/...> works
	$(DOCKER) $(OS_HOSTCC) -E -P -x assembler-with-cpp -nostdinc -undef -D__DTS__ \
	  -I $(SDK_PATH) \
	  -I $(LINUX_PATH)/include \
	  -I $(LINUX_PATH)/arch/$(ARCH)/boot/dts \
	  -I $(LINUX_PATH)/arch/$(ARCH)/boot/dts/xilinx \
	  -o $(TMP_OS_PATH)/board-overlay/board.pp $(TMP_OS_PATH)/board-overlay/board.dtso
	$(DOCKER) $(DTC_BIN) -@ -I dts -O dtb -b 0 \
	  -i $(LINUX_PATH)/arch/$(ARCH)/boot/dts -i $(LINUX_PATH)/arch/$(ARCH)/boot/dts/xilinx \
	  -o $@ $(TMP_OS_PATH)/board-overlay/board.pp
	$(call ok,$@)

###############################################################################
# uImage / Image
###############################################################################

$(TMP_OS_PATH)/$(KERNEL_BIN): $(LINUX_BUILD_STAMP) $(LINUX_SELECTION_STAMP) | $(TMP_OS_PATH)/
	cp "$(LINUX_PATH)/arch/$(ARCH)/boot/$(KERNEL_BIN)" "$@"

###############################################################################
# kernel.itb
###############################################################################


define ITS_TEMPLATE
/dts-v1/;
/ {
  description = "Linux + DTB (FIT)";
  #address-cells = <1>;
  images {
    kernel {
      description = "Linux kernel";
      data = /incbin/("$(KERNEL_BIN)");
      type = "kernel";
      arch = "$(ARCH)";
      os = "linux";
      compression = "none";
      load = <0x03000000>;
      entry = <0x03000000>;
      hash-1 { algo = "sha256"; };
    };
    fdt {
      description = "Base Device Tree";
      data = /incbin/("devicetree.dtb");
      type = "flat_dt";
      arch = "$(ARCH)";
      compression = "none";
      load = <0x07000000>;
      hash-1 { algo = "sha256"; };
    };
    overlay_board {
      description = "Board overlay";
      data = /incbin/("board-overlay/board.dtbo");
      type = "flat_dt";
      arch = "$(ARCH)";
      compression = "none";
      load = <0x07100000>;
      hash-1 { algo = "sha256"; };
    };
  };
  configurations {
    default = "conf";
    conf {
      description = "Boot Linux kernel";
      kernel = "kernel";
      fdt = "fdt", "overlay_board";
      hash-1 { algo = "sha256"; };
    };
  };
};
endef

$(TMP_OS_PATH)/kernel.its: $(TMP_OS_PATH)/$(KERNEL_BIN) $(TMP_OS_PATH)/devicetree.dtb $(TMP_OS_PATH)/board-overlay/board.dtbo $(OS_PATH)/os.mk | $(TMP_OS_PATH)/
	@$(file >$@,$(ITS_TEMPLATE))
	$(call ok,$@)

$(TMP_OS_PATH)/kernel.itb: $(TMP_OS_PATH)/kernel.its | $(TMP_OS_PATH)/
	$(DOCKER) mkimage -D "-i $(TMP_OS_PATH)" -f $< $@
	$(call ok,$@)

###############################################################################
#
###############################################################################

OS_FILES := \
  $(INSTRUMENT_ZIP) \
  $(API_FILES) \
  $(WWW_ASSETS) \
  $(TMP_OS_PATH)/$(BOOT_BIN) \
  $(TMP_OS_PATH)/kernel.itb \
  $(TMP_OS_PATH)/devicetree.dtb

.PHONY: os
os: $(OS_FILES)

.PHONY: clean_os
clean_os:
	rm -rf $(TMP_OS_PATH)
