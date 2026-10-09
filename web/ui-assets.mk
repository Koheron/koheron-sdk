# Shared, checksum-verified UI dependencies. Changing this file or the manifest
# refreshes existing outputs, including artifacts cached under the old versions.
ifndef KOHERON_UI_ASSETS_INCLUDED
KOHERON_UI_ASSETS_INCLUDED := 1
UI_ASSETS_MK := $(WEB_PATH)/ui-assets.mk
UI_ASSETS_CHECKSUMS := $(WEB_PATH)/ui-assets.sha256
UI_ASSETS_DOWNLOADER := $(WEB_PATH)/../os/scripts/download_verified.sh

# Instantiate for the instrument directory and the OS dashboard directory.
define ui_asset_rules
$(1)/jquery.min.js: $(UI_ASSETS_MK) $(UI_ASSETS_CHECKSUMS) $(UI_ASSETS_DOWNLOADER)
	bash "$(UI_ASSETS_DOWNLOADER)" "$(UI_ASSETS_CHECKSUMS)" "$$@" "https://code.jquery.com/jquery-3.7.1.min.js"
	touch "$$@"

$(1)/bootstrap.min.js: $(UI_ASSETS_MK) $(UI_ASSETS_CHECKSUMS) $(UI_ASSETS_DOWNLOADER)
	bash "$(UI_ASSETS_DOWNLOADER)" "$(UI_ASSETS_CHECKSUMS)" "$$@" "https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/js/bootstrap.min.js"
	touch "$$@"

$(1)/bootstrap.min.css: $(UI_ASSETS_MK) $(UI_ASSETS_CHECKSUMS) $(UI_ASSETS_DOWNLOADER)
	bash "$(UI_ASSETS_DOWNLOADER)" "$(UI_ASSETS_CHECKSUMS)" "$$@" "https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/css/bootstrap.min.css"
	touch "$$@"

$(1)/glyphicons-halflings-regular.woff2: $(UI_ASSETS_MK) $(UI_ASSETS_CHECKSUMS) $(UI_ASSETS_DOWNLOADER)
	bash "$(UI_ASSETS_DOWNLOADER)" "$(UI_ASSETS_CHECKSUMS)" "$$@" "https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/fonts/glyphicons-halflings-regular.woff2"
	touch "$$@"
endef
endif
