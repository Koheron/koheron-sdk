###############################################################################
# Build and run the instrument: $ make NAME=spectrum HOST=192.168.1.12 run
###############################################################################

NAME = oscillo
HOST = 192.168.1.100

###############################################################################
# Get the project configuration
# MAKE_PY script parses the properties defined MAIN_YML
###############################################################################

MAIN_YML = projects/$(NAME)/main.yml
MAKE_PY = scripts/make.py

# Store all build artifacts in TMP
TMP = tmp

# properties defined MAIN_YML :
BOARD:=$(shell set -e; python $(MAKE_PY) --board $(NAME) && cat $(TMP)/$(NAME).board)
CORES:=$(shell set -e; python $(MAKE_PY) --cores $(NAME) && cat $(TMP)/$(NAME).cores)
DRIVERS:=$(shell set -e; python $(MAKE_PY) --drivers $(NAME) && cat $(TMP)/$(NAME).drivers)
XDC:=$(shell set -e; python $(MAKE_PY) --xdc $(NAME) && cat $(TMP)/$(NAME).xdc)

PART:=`cat boards/$(BOARD)/PART`
PATCHES = boards/$(BOARD)/patches
PROC = ps7_cortexa9_0

# Custom commands
VIVADO_VERSION = 2016.2
VIVADO = vivado -nolog -nojournal -mode batch
HSI = hsi -nolog -nojournal -mode batch
RM = rm -rf

DOCKER ?= False
ifeq ($(DOCKER),True)
	PYTHON=/usr/bin/python
else
	PYTHON=$(TCP_SERVER_VENV)/bin/python
endif

###############################################################################
# Linux and U-boot
###############################################################################

# solves problem with awk while building linux kernel
# solution taken from http://www.googoolia.com/wp/2015/04/21/awk-symbol-lookup-error-awk-undefined-symbol-mpfr_z_sub/
LD_LIBRARY_PATH =

UBOOT_TAG = xilinx-v$(VIVADO_VERSION)
LINUX_TAG = xilinx-v$(VIVADO_VERSION)
DTREE_TAG = xilinx-v$(VIVADO_VERSION)

UBOOT_DIR = $(TMP)/u-boot-xlnx-$(UBOOT_TAG)
LINUX_DIR = $(TMP)/linux-xlnx-$(LINUX_TAG)
DTREE_DIR = $(TMP)/device-tree-xlnx-$(DTREE_TAG)

UBOOT_TAR = $(TMP)/u-boot-xlnx-$(UBOOT_TAG).tar.gz
LINUX_TAR = $(TMP)/linux-xlnx-$(LINUX_TAG).tar.gz
DTREE_TAR = $(TMP)/device-tree-xlnx-$(DTREE_TAG).tar.gz

UBOOT_URL = https://github.com/Xilinx/u-boot-xlnx/archive/$(UBOOT_TAG).tar.gz
LINUX_URL = https://github.com/Xilinx/linux-xlnx/archive/$(LINUX_TAG).tar.gz
DTREE_URL = https://github.com/Xilinx/device-tree-xlnx/archive/$(DTREE_TAG).tar.gz

LINUX_CFLAGS = "-O2 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
UBOOT_CFLAGS = "-O2 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
ARMHF_CFLAGS = "-O2 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"

RTL_TAR = $(TMP)/rtl8192cu.tgz
RTL_URL = https://googledrive.com/host/0B-t5klOOymMNfmJ0bFQzTVNXQ3RtWm5SQ2NGTE1hRUlTd3V2emdSNzN6d0pYamNILW83Wmc/rtl8192cu/rtl8192cu.tgz

# Project configuration

CONFIG_TCL = $(TMP)/$(NAME).config.tcl
TEMPLATE_DIR = scripts/templates

# Versioning
VERSION_FILE = $(TMP)/$(NAME).version
VERSION = $(shell cat $(VERSION_FILE))
SHA_FILE = $(TMP)/$(NAME).sha
SHA = $(shell cat $(SHA_FILE))

# Zip
TCP_SERVER_DIR = $(TMP)/$(NAME).tcp-server
TCP_SERVER = $(TCP_SERVER_DIR)/tmp/kserverd
SERVER_CONFIG = projects/$(NAME)/drivers.yml
TCP_SERVER_SHA = devmem
TCP_SERVER_VENV = $(TMP)/$(NAME).tcp_server_venv
TCP_SERVER_MIDDLEWARE = $(TMP)/$(NAME).middleware

START_SH = $(TMP)/$(NAME).start.sh

ZIP = $(TMP)/$(NAME)-$(VERSION).zip

# App
S3_URL = http://zynq-sdk.s3-website-eu-west-1.amazonaws.com
STATIC_SHA := $(shell curl -s $(S3_URL)/apps | cut -d" " -f1)
STATIC_URL = $(S3_URL)/app-$(STATIC_SHA).zip
STATIC_ZIP = $(TMP)/static.zip

HTTP_API_SRC = $(wildcard os/api/*)
HTTP_API_DIR = $(TMP)/app
HTTP_API_DRIVERS = common eeprom laser
HTTP_API_DRIVERS_DIR = $(HTTP_API_DIR)/api_app/drivers
HTTP_API_ZIP = app-$(VERSION).zip

METADATA = $(TMP)/metadata.json

.PRECIOUS: $(TMP)/cores/% $(TMP)/%.xpr $(TMP)/%.hwdef $(TMP)/%.bit $(TMP)/%.fsbl/executable.elf $(TMP)/%.tree/system.dts

.PHONY: all help \
        test_module test_core test_% test test_app test_instrum test_all \
        server xpr zip app bd http_api \
        run app_sync app_sync_ssh tcp-server_cli

all: $(ZIP) $(STATIC_ZIP) $(HTTP_API_ZIP) boot.bin uImage devicetree.dtb fw_printenv tcp-server_cli

###############################################################################
# API
###############################################################################

help:
	@echo - server: Build the server
	@echo - bd: Build the block design interactively
	@echo - xpr: Build the Vivado project
	@echo - bit: Build the bitstream
	@echo - http: Build the HTTP API
	@echo - run: Run the instrument
	@echo - test: Test the instrument

$(TMP):
	mkdir -p $(TMP)

# Run Vivado interactively and build block design
bd: $(CONFIG_TCL) $(XDC) projects/$(NAME)/*.tcl $(addprefix $(TMP)/cores/, $(CORES))
	vivado -nolog -nojournal -source scripts/block_design.tcl -tclargs $(NAME) $(PART) $(BOARD)

test_module: $(CONFIG_TCL) projects/$(NAME)/*.tcl $(addprefix $(TMP)/cores/, $(CORES))
	vivado -source scripts/test_module.tcl -tclargs $(NAME) $(PART)

test_core:
	vivado -source scripts/test_core.tcl -tclargs $(CORE) $(PART)

test_%: tests/tests_%.py
	py.test -v $<

test: tests/$(NAME).py
	python $<

test_app: | app_sync test_instrument_manager

test_instrum: test_device_memory test_common test_gpio test_$(NAME)

test_all: | test_app test_instrum

server: $(TCP_SERVER)
xpr: $(TMP)/$(NAME).xpr
bit: $(TMP)/$(NAME).bit
zip: $(ZIP)
http: $(HTTP_API_ZIP)

run: $(ZIP)
	curl -v -F $(NAME)-$(VERSION).zip=@$(ZIP) http://$(HOST)/api/instruments/upload
	curl http://$(HOST)/api/instruments/run/$(NAME)/$(VERSION)

###############################################################################
# versioning
###############################################################################

$(VERSION_FILE): | $(TMP)
	echo $(shell (git rev-parse --short HEAD || echo "default")) > $@

$(SHA_FILE): $(VERSION_FILE)
	echo $(shell (printf $(NAME)-$(VERSION) | sha256sum | sed 's/\W//g')) > $@

###############################################################################
# FPGA
###############################################################################

$(CONFIG_TCL): $(MAKE_PY) $(MAIN_YML) $(SHA_FILE) $(TEMPLATE_DIR)/config.tcl
	python $(MAKE_PY) --config_tcl $(NAME)

$(TMP)/cores/%: fpga/cores/%/core_config.tcl fpga/cores/%/*.v
	mkdir -p $(@D)
	$(VIVADO) -source scripts/core.tcl -tclargs $* $(PART)

$(TMP)/$(NAME).xpr: $(CONFIG_TCL) $(XDC) projects/$(NAME)/*.tcl $(addprefix $(TMP)/cores/, $(CORES))
	mkdir -p $(@D)
	$(VIVADO) -source scripts/project.tcl -tclargs $(NAME) $(PART) $(BOARD)
	@echo [$@] OK

$(TMP)/$(NAME).bit: $(TMP)/$(NAME).xpr
	mkdir -p $(@D)
	$(VIVADO) -source scripts/bitstream.tcl -tclargs $(NAME)
	@echo [$@] OK

###############################################################################
# first-stage boot loader
###############################################################################

$(TMP)/$(NAME).fsbl/executable.elf: $(TMP)/$(NAME).hwdef
	mkdir -p $(@D)
	$(HSI) -source scripts/fsbl.tcl -tclargs $(NAME) $(PROC)

###############################################################################
# U-Boot
###############################################################################

$(UBOOT_TAR):
	mkdir -p $(@D)
	curl -L $(UBOOT_URL) -o $@

$(UBOOT_DIR): $(UBOOT_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	patch -d $(TMP) -p 0 < $(PATCHES)/u-boot-xlnx-$(UBOOT_TAG).patch
	bash $(PATCHES)/uboot.sh $(PATCHES) $@

$(TMP)/u-boot.elf: $(UBOOT_DIR)
	mkdir -p $(@D)
	make -C $< mrproper
	make -C $< arch=arm `find $(PATCHES) -name '*_defconfig' -exec basename {} \;`
	make -C $< arch=arm CFLAGS=$(UBOOT_CFLAGS) \
	  CROSS_COMPILE=arm-linux-gnueabihf- all
	cp $</u-boot $@

fw_printenv: $(UBOOT_DIR) $(TMP)/u-boot.elf
	make -C $< arch=ARM CFLAGS=$(ARMHF_CFLAGS) \
	  CROSS_COMPILE=arm-linux-gnueabihf- env
	cp $</tools/env/fw_printenv $@

###############################################################################
# boot.bin
###############################################################################

boot.bin: $(TMP)/$(NAME).fsbl/executable.elf $(TMP)/$(NAME).bit $(TMP)/u-boot.elf
	echo "img:{[bootloader] $^}" > $(TMP)/boot.bif
	bootgen -image $(TMP)/boot.bif -w -o i $@

###############################################################################
# device tree
###############################################################################

$(TMP)/$(NAME).hwdef: $(TMP)/$(NAME).xpr
	mkdir -p $(@D)
	$(VIVADO) -source scripts/hwdef.tcl -tclargs $(NAME)

$(DTREE_TAR):
	mkdir -p $(@D)
	curl -L $(DTREE_URL) -o $@

$(DTREE_DIR): $(DTREE_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@

$(TMP)/$(NAME).tree/system.dts: $(TMP)/$(NAME).hwdef $(DTREE_DIR)
	mkdir -p $(@D)
	$(HSI) -source scripts/devicetree.tcl -tclargs $(NAME) $(PROC) $(DTREE_DIR) $(VIVADO_VERSION)
	patch $@ $(PATCHES)/devicetree.patch

###############################################################################
# Linux
###############################################################################

$(RTL_TAR):
	mkdir -p $(@D)
	curl -L $(RTL_URL) -o $@

$(LINUX_TAR):
	mkdir -p $(@D)
	curl -L $(LINUX_URL) -o $@

$(LINUX_DIR): $(LINUX_TAR) $(RTL_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	tar -zxf $(RTL_TAR) --directory=$@/drivers/net/wireless/realtek
	patch -d $(TMP) -p 0 < $(PATCHES)/linux-xlnx-$(LINUX_TAG).patch
	bash $(PATCHES)/linux.sh $(PATCHES) $@

uImage: $(LINUX_DIR)
	make -C $< mrproper
	make -C $< ARCH=arm xilinx_zynq_defconfig
	make -C $< ARCH=arm CFLAGS=$(LINUX_CFLAGS) \
	  -j $(shell nproc 2> /dev/null || echo 1) \
	  CROSS_COMPILE=arm-linux-gnueabihf- UIMAGE_LOADADDR=0x8000 uImage
	cp $</arch/arm/boot/uImage $@

devicetree.dtb: uImage $(TMP)/$(NAME).tree/system.dts
	$(LINUX_DIR)/scripts/dtc/dtc -I dts -O dtb -o devicetree.dtb \
	  -i $(TMP)/$(NAME).tree $(TMP)/$(NAME).tree/system.dts

###############################################################################
# tcp-server (compiled with project specific middleware)
###############################################################################

$(TCP_SERVER_DIR):
	git clone https://github.com/Koheron/tcp-server.git $(TCP_SERVER_DIR)
	cd $(TCP_SERVER_DIR) && git checkout $(TCP_SERVER_SHA)
	echo `cd $(TCP_SERVER_DIR) && git rev-parse HEAD` > $(TCP_SERVER_DIR)/VERSION

$(TCP_SERVER_DIR)/requirements.txt: $(TCP_SERVER_DIR)

$(TCP_SERVER_VENV): $(TCP_SERVER_DIR)/requirements.txt
ifeq ($(DOCKER),True)
	/usr/bin/pip install -r $(TCP_SERVER_DIR)/requirements.txt
else
	virtualenv $(TCP_SERVER_VENV)
	$(TCP_SERVER_VENV)/bin/pip install -r $(TCP_SERVER_DIR)/requirements.txt
endif

$(TCP_SERVER_MIDDLEWARE)/%: %
	mkdir -p -- `dirname -- $@`
	cp $^ $@

$(TCP_SERVER): $(MAKE_PY) $(TCP_SERVER_VENV) $(SERVER_CONFIG) \
               $(addprefix $(TCP_SERVER_MIDDLEWARE)/, $(DRIVERS)) \
               drivers/lib projects/default/server.yml
	python $(MAKE_PY) --middleware $(NAME)
	cp -R drivers/lib $(TCP_SERVER_MIDDLEWARE)/drivers/
	cd $(TCP_SERVER_DIR) && make CONFIG=$(SERVER_CONFIG) BASE_DIR=../.. \
	  PYTHON=$(PYTHON) MIDWARE_PATH=$(TCP_SERVER_MIDDLEWARE) clean all
	@echo [$@] OK

tcp-server_cli: $(TCP_SERVER_DIR) $(TCP_SERVER_VENV)
	cd $(TCP_SERVER_DIR) && make CONFIG=$(SERVER_CONFIG) BASE_DIR=../.. PYTHON=$(PYTHON) cli

###############################################################################
# Instrument ZIP file (contains bitstream, tcp-server)
###############################################################################

$(START_SH): $(MAKE_PY) $(MAIN_YML) $(TEMPLATE_DIR)/start.sh
	python $(MAKE_PY) --start_sh $(NAME)

$(ZIP): $(TCP_SERVER) $(VERSION_FILE) $(PYTHON_DIR) $(TMP)/$(NAME).bit $(START_SH)
	zip --junk-paths $(ZIP) $(TMP)/$(NAME).bit $(TCP_SERVER) $(START_SH)

###############################################################################
# HTTP API
###############################################################################

$(METADATA): $(MAKE_PY) $(VERSION_FILE)
	python $(MAKE_PY) --metadata $(NAME) $(VERSION)

$(HTTP_API_DRIVERS_DIR)/%: drivers/%/__init__.py
	mkdir -p $@
	cp $< $@/__init__.py

$(HTTP_API_DIR): $(HTTP_API_SRC) $(METADATA) $(addprefix $(HTTP_API_DRIVERS_DIR)/, $(HTTP_API_DRIVERS))
	touch $(HTTP_API_DRIVERS_DIR)/__init__.py
	mkdir -p $(HTTP_API_DIR)/api_app
	cp -R os/api/. $(HTTP_API_DIR)/api_app
	cp $(TMP)/metadata.json $(HTTP_API_DIR)
	cp os/wsgi.py $(HTTP_API_DIR)

$(HTTP_API_ZIP): $(HTTP_API_DIR)
	cd $(HTTP_API_DIR) && zip -r $(HTTP_API_ZIP) .

app_sync: $(HTTP_API_ZIP)
	curl -v -F app-$(VERSION).zip=@$(HTTP_API_DIR)/$(HTTP_API_ZIP) http://$(HOST)/api/app/update

# To use if uwsgi is not running
app_sync_ssh: $(HTTP_API_ZIP)
	rsync -avz -e "ssh -i /ssh-private-key" $(HTTP_API_DIR)/. root@$(HOST):/usr/local/flask/

###############################################################################
# Static content served by NGINX
###############################################################################

$(STATIC_ZIP): $(TMP)
	echo $(STATIC_SHA)
	curl -L $(STATIC_URL) -o $(STATIC_ZIP)

###############################################################################
# clean
###############################################################################

.PHONY: clean clean_server clean_cores clean_xpr clean_all

clean:
	$(RM) $(TMP)/$(NAME).* $(TMP)/$(NAME)-*.zip

clean_server:
	$(RM) $(TCP_SERVER_DIR) $(TCP_SERVER_MIDDLEWARE)

clean_cores:
	$(RM) $(TMP)/cores

clean_xpr:
	$(RM) $(TMP)/$(NAME).xpr
	
clean_all:
	$(RM) uImage fw_printenv boot.bin devicetree.dtb $(TMP)
	$(RM) .Xil usage_statistics_webtalk.html usage_statistics_webtalk.xml
	$(RM) webtalk*.log webtalk*.jou



