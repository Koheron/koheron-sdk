ABS_TMP_OS_PATH := $(abspath $(TMP_OS_PATH))

TMP_OS_BOARD_PATH ?= $(TMP_PROJECT_PATH)/os

UBOOT_TAG ?= xilinx-uboot-v$(VIVADO_VERSION)
DTREE_TAG ?= xilinx_v$(VIVADO_VERSION)

UBOOT_URL := https://github.com/Xilinx/u-boot-xlnx/archive/xilinx-v$(VIVADO_VERSION).tar.gz
DTREE_URL := https://github.com/Xilinx/device-tree-xlnx/archive/refs/tags/$(DTREE_TAG).tar.gz

include $(OS_PATH)/rootfs.mk

UBOOT_PATH ?= $(TMP_OS_BOARD_PATH)/u-boot-xlnx-$(UBOOT_TAG)
DTREE_PATH := $(TMP_OS_PATH)/device-tree-xlnx-$(DTREE_TAG)

UBOOT_TAR := $(TMP)/u-boot-xlnx-$(UBOOT_TAG).tar.gz
DTREE_TAR := $(TMP)/device-tree-xlnx-$(DTREE_TAG).tar.gz

BOOT_MEDIUM ?= mmcblk0

###############################################################################
# fsbl/executable.elf
###############################################################################

# Additional files (including fsbl_hooks.c) can be added to the FSBL in $(BOARD_PATH)/patches/fsbl
ifndef FSBL_PATH
FSBL_PATH := $(BOARD_PATH)/patches/fsbl
endif
FSBL_FILES := $(wildcard $(FSBL_PATH)/*.h $(FSBL_PATH)/*.c)

.PHONY: fsbl
fsbl: $(TMP_OS_PATH)/fsbl/executable.elf

$(TMP_OS_PATH)/hard/$(NAME).xsa: $(TMP_FPGA_PATH)/$(NAME).xsa | $(TMP_OS_PATH)/hard/
	cp $< $@
	$(call ok,$@)

$(TMP_OS_PATH)/fsbl/Makefile:  $(TMP_OS_PATH)/hard/$(NAME).xsa | $(TMP_OS_PATH)/fsbl/
	$(HSI) $(FPGA_PATH)/hsi/fsbl.tcl $(NAME) $(PROC) $(TMP_OS_PATH)/hard $(@D) $< $(ZYNQ_TYPE)
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

$(UBOOT_TAR):
	mkdir -p $(@D)
	curl -L $(UBOOT_URL) -o $@
	$(call ok,$@)

$(UBOOT_PATH)/.unpacked: $(UBOOT_TAR) | $(UBOOT_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	touch $@
	$(call ok,$@)

UBOOT_PATCH_FILES := $(shell test -d $(PATCHES)/u-boot && find $(PATCHES)/u-boot -type f)

# Configure U-Boot once to avoid concurrent defconfig/mrproper races
UBOOT_CONFIG_STAMP := $(UBOOT_PATH)/.config

$(UBOOT_CONFIG_STAMP): $(UBOOT_PATH)/.unpacked $(UBOOT_PATCH_FILES)
	cp -a $(PATCHES)/${UBOOT_CONFIG} $(UBOOT_PATH)/ 2>/dev/null || true
	cp -a $(PATCHES)/u-boot/. $(UBOOT_PATH)/ 2>/dev/null || true
	$(DOCKER) make -C $(UBOOT_PATH) mrproper
	$(DOCKER) make -C $(UBOOT_PATH) ARCH=$(UBOOT_ARCH) $(UBOOT_CONFIG)
	@touch $@
	$(call ok,$@)

$(TMP_OS_BOARD_PATH)/u-boot.elf: $(UBOOT_CONFIG_STAMP) | $(TMP_BOARD_PATH)/
	$(DOCKER) make -C $(UBOOT_PATH) ARCH=$(UBOOT_ARCH) CFLAGS="$(UBOOT_CFLAGS) $(GCC_FLAGS)" \
	  CROSS_COMPILE=$(GCC_ARCH)- all
	if [ -f $(UBOOT_PATH)/u-boot.elf ]; then cp $(UBOOT_PATH)/u-boot.elf $@; else cp $(UBOOT_PATH)/u-boot $@; fi
	$(call ok,$@)

$(TMP_OS_PATH)/u-boot.elf: $(TMP_OS_BOARD_PATH)/u-boot.elf | $(TMP_OS_PATH)/
	cp $< $@
	$(call ok,$@)

###############################################################################
# pmu/executable.elf
###############################################################################

.PHONY: pmufw
pmufw: $(TMP_OS_PATH)/pmu/pmufw.elf

$(TMP_OS_PATH)/pmu/Makefile: $(TMP_FPGA_PATH)/$(NAME).xsa
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/pmufw.tcl $(NAME) $(TMP_OS_PATH)/hard $(@D) $<
	$(call ok,$@)

$(TMP_OS_PATH)/pmu/executable.elf: $(TMP_OS_PATH)/pmu/Makefile
	source $(VITIS_PATH)/settings64.sh && $(DOCKER) make -C $(@D) all

.PHONY: clean_pmufw
clean_pmufw:
	rm -rf $(TMP_OS_PATH)/pmu

###############################################################################
# bl31.elf
###############################################################################

$(ATRUST_TAR):
	mkdir -p $(@D)
	curl -L $(ARMTRUST_URL) -o $@
	$(call ok,$@)

$(ATRUST_PATH)/.unpacked: $(ATRUST_TAR) | $(ATRUST_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	$(call ok,$@)

$(TMP_OS_PATH)/bl31.elf: $(ATRUST_PATH)/.unpacked
	$(DOCKER) make CROSS_COMPILE=$(GCC_ARCH)- PLAT=zynqmp bl31 ZYNQMP_ATF_MEM_BASE=0x10000 ZYNQMP_ATF_MEM_SIZE=0x40000 -C $(ATRUST_PATH)
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
	echo "img:{ [fsbl_config] a53_x64" > $(TMP_OS_PATH)/boot.bif
	echo "[pmufw_image] $(TMP_OS_PATH)/pmu/executable.elf" >> $(TMP_OS_PATH)/boot.bif
	echo "[bootloader] $(TMP_OS_PATH)/fsbl/executable.elf" >> $(TMP_OS_PATH)/boot.bif
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

$(DTREE_TAR):
	mkdir -p $(@D)
	curl -L $(DTREE_URL) -o $@
	$(call ok,$@)

$(DTREE_PATH)/.unpacked: $(DTREE_TAR) | $(DTREE_PATH)/
	tar -zxf $< --strip-components=1 -C $(@D)
	@touch $@
	$(call ok,$@)

.PHONY: devicetree
devicetree: $(TMP_OS_PATH)/devicetree/system-top.dts

$(TMP_OS_PATH)/devicetree/system-top.dts: $(TMP_OS_PATH)/hard/$(NAME).xsa $(DTREE_PATH)/.unpacked
	mkdir -p $(@D)
	$(HSI) $(FPGA_PATH)/hsi/devicetree.tcl $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VERSION) $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/devicetree $< $(BOOT_MEDIUM)
	$(call ok,$@)

$(TMP_OS_PATH)/devicetree.dtb: $(DTC_BIN)  $(TMP_OS_PATH)/devicetree/system-top.dts
	$(DOCKER) gcc -I $(TMP_OS_PATH)/devicetree/ -I $(TMP_OS_PATH)/devicetree/include/ -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp -o \
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

$(TMP_OS_PATH)/pl-overlay/memory.dtsi: $(MEMORY_YML) $(FPGA_PATH)/memory.dtsi | $(TMP_OS_PATH)/pl-overlay/
	$(MAKE_PY) --memory_dtsi $@ $<
	$(call ok,$@)

OVERRIDE_DTSI ?= $(FPGA_PATH)/override.dtsi

$(TMP_OS_PATH)/pl-overlay/override.dtsi: $(OVERRIDE_DTSI) | $(TMP_OS_PATH)/pl-overlay/
	cp $< $@
	$(call ok,$@)

$(TMP_OS_PATH)/pl-overlay/pl_wrap.dts: $(FPGA_PATH)/pl_wrap.dts | $(TMP_OS_PATH)/pl-overlay/
	cp $< $@
	$(call ok,$@)

$(TMP_OS_PATH)/pl.dtbo: $(DTC_BIN) \
  $(TMP_OS_PATH)/pl-overlay/pl.dtsi \
  $(TMP_OS_PATH)/pl-overlay/memory.dtsi \
  $(TMP_OS_PATH)/pl-overlay/override.dtsi \
  $(TMP_OS_PATH)/pl-overlay/pl_wrap.dts
	sed -i 's/".bin"/"$(NAME).bit.bin"/g' $(TMP_OS_PATH)/pl-overlay/pl.dtsi
	$(DOCKER) $(DTC_BIN) -@ -I dts -O dtb -b 0 \
	  -i $(TMP_OS_PATH)/pl-overlay \
	  -o $@ $(TMP_OS_PATH)/pl-overlay/pl_wrap.dts
	$(call ok,$@)

$(TMP_PROJECT_PATH)/pl.dtbo: $(TMP_OS_PATH)/pl.dtbo
	cp $(TMP_OS_PATH)/pl.dtbo  $(TMP_PROJECT_PATH)/pl.dtbo

###############################################################################
# board.dtbo
###############################################################################

BOARD_DTSO ?= $(OS_PATH)/board.dtso

$(TMP_OS_PATH)/board-overlay/board.dtso: $(BOARD_DTSO) | $(TMP_OS_PATH)/board-overlay/
	cp $< $@
	$(call ok,$@)

$(TMP_OS_PATH)/board-overlay/board.dtbo: $(TMP_OS_PATH)/board-overlay/board.dtso $(LINUX_BUILD_STAMP)
	# Preprocess so #include <dt-bindings/...> works
	$(DOCKER) gcc -E -P -x assembler-with-cpp -nostdinc -undef -D__DTS__ \
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

$(TMP_OS_PATH)/$(KERNEL_BIN): $(LINUX_BUILD_STAMP) | $(TMP_OS_PATH)/
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
      data = /incbin/("$(ABS_TMP_OS_PATH)/$(KERNEL_BIN)");
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
      data = /incbin/("$(ABS_TMP_OS_PATH)/devicetree.dtb");
      type = "flat_dt";
      arch = "$(ARCH)";
      compression = "none";
	  load = <0x07000000>;
      hash-1 { algo = "sha256"; };
    };
    overlay_board {
      description = "Board overlay";
      data = /incbin/("$(ABS_TMP_OS_PATH)/board-overlay/board.dtbo");
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

$(TMP_OS_PATH)/kernel.its: $(TMP_OS_PATH)/$(KERNEL_BIN) $(TMP_OS_PATH)/devicetree.dtb $(TMP_OS_PATH)/board-overlay/board.dtbo | $(TMP_OS_PATH)/
	@$(file >$@,$(ITS_TEMPLATE))
	$(call ok,$@)

$(TMP_OS_PATH)/kernel.itb: $(TMP_OS_PATH)/kernel.its | $(TMP_OS_PATH)/
	mkimage -f $< $@
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

.PHONY: base-rootfs
base-rootfs: $(BASE_ROOTFS_TAR)

.PHONY: clean-base-rootfs
clean-base-rootfs:
	rm -f $(BASE_ROOTFS_TAR)

$(BASE_ROOTFS_TAR): \
  $(OS_PATH)/scripts/build_base_rootfs_tar.sh \
  $(OS_PATH)/scripts/chroot_base_rootfs.sh \
  $(ROOT_TAR_PATH)
	@mkdir -p $(@D)
	@test -s "$(ROOT_TAR_PATH)" || { echo "Missing root tar: $(ROOT_TAR_PATH)"; exit 1; }
	# Optional envs: TIMEZONE, PASSWD
	$(DOCKER_ROOT) bash $(OS_PATH)/scripts/build_base_rootfs_tar.sh \
	  "$(ROOT_TAR_PATH)" "$@" "$(QEMU_BIN)"
	$(call ok,$@)

EXTLINUX_CONF ?= $(OS_PATH)/extlinux.conf

$(RELEASE_ZIP): $(BASE_ROOTFS_TAR) \
  $(OS_FILES) \
  $(OS_PATH)/scripts/build_image.sh \
  $(OVERLAY_TAR) $(MANIFEST_TXT) $(EXTLINUX_CONF) \
  $(OS_PATH)/scripts/chroot_overlay.sh
	@mkdir -p $(@D)
	@test -s "$(OVERLAY_TAR)" || { echo "Missing overlay tar: $(OVERLAY_TAR)"; exit 1; }
	$(DOCKER_ROOT) env BASE_ROOTFS_TAR="$(BASE_ROOTFS_TAR)" \
		EXTLINUX_CONF="$(EXTLINUX_CONF)" \
		bash $(OS_PATH)/scripts/build_image.sh \
		"$(TMP_PROJECT_PATH)" "$(OS_PATH)" "$(TMP_OS_PATH)" \
		"$(ROOT_TAR_PATH)" "$(OVERLAY_TAR)" "$(QEMU_BIN)" \
		"$(RELEASE_NAME)"
	$(call ok,$@)

# Build image
.PHONY: image
image: $(RELEASE_ZIP)

# Flash image on SD card
FLASH_ALLOWED_VENDORS ?= TS-RDF5 TS-RDF5A
FLASH_ALLOWED_MODELS  ?= SD_Transcend Transcend
FLASH_DEVICES         ?=
FLASH_DEVICE          ?=

.PHONY: flash
flash:
	FLASH_ALLOWED_VENDORS="$(FLASH_ALLOWED_VENDORS)" \
	FLASH_ALLOWED_MODELS="$(FLASH_ALLOWED_MODELS)" \
	FLASH_DEVICES="$(FLASH_DEVICES)" \
	FLASH_DEVICE="$(FLASH_DEVICE)" \
	python3 $(OS_PATH)/scripts/flash_all.py $(TMP_PROJECT_PATH)/$(RELEASE_NAME).zip