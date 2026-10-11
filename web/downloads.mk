
WEB_DOWNLOADS := $(TMP_WEB_PATH)/_koheron.css
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.resize.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.selection.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.time.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.axislabels.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.canvas.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/bootstrap.min.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/bootstrap.min.css
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.min.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/_koheron_logo.svg
WEB_DOWNLOADS += $(TMP_WEB_PATH)/_koheron.png
WEB_DOWNLOADS += $(TMP_WEB_PATH)/kbird.ico
WEB_DOWNLOADS += $(TMP_WEB_PATH)/lato-v11-latin-400.woff2
WEB_DOWNLOADS += $(TMP_WEB_PATH)/lato-v11-latin-700.woff2
WEB_DOWNLOADS += $(TMP_WEB_PATH)/lato-v11-latin-900.woff2
WEB_DOWNLOADS += $(TMP_WEB_PATH)/glyphicons-halflings-regular.woff2
WEB_DOWNLOADS += $(TMP_WEB_PATH)/html-imports.min.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/html-imports.min.js.map
WEB_DOWNLOADS += $(TMP_WEB_PATH)/navigation.html

include $(WEB_PATH)/ui-assets.mk
$(eval $(call ui_asset_rules,$(TMP_WEB_PATH)))

# Owned Canvas 2D plotting stack, built from readable local sources.
PLOT_BUILD ?= $(WEB_DOCKER_RUN) node $(WEB_PATH)/plotting/build.cjs
PLOT_ASSETS := $(addprefix $(TMP_WEB_PATH)/,jquery.flot.js jquery.flot.resize.js jquery.flot.selection.js jquery.flot.time.js jquery.flot.axislabels.js jquery.flot.canvas.js flot-LICENSE.txt)
WEB_DOWNLOADS += $(TMP_WEB_PATH)/flot-LICENSE.txt
$(PLOT_ASSETS) &: $(wildcard $(WEB_PATH)/plotting/src/*.js) $(WEB_PATH)/plotting/build.cjs $(WEB_PATH)/plotting/licenses/LICENSE.txt $(WEB_PATH)/plotting/package.json $(WEB_PATH)/plotting/package-lock.json
	$(PLOT_BUILD) $(TMP_WEB_PATH)

WEB_ASSET_DOWNLOADER := $(WEB_PATH)/download_asset.sh

$(TMP_WEB_PATH)/_koheron.css: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://assets.koheron.com/css/main.css"

$(TMP_WEB_PATH)/_koheron.png: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://assets.koheron.com/images/logo/koheron.png"

$(TMP_WEB_PATH)/_koheron_logo.svg: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://assets.koheron.com/images/logo/koheron_logo.svg"

$(TMP_WEB_PATH)/kbird.ico: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://assets.koheron.com/images/logo/koheron.ico"

$(TMP_WEB_PATH)/lato-v11-latin-400.woff2: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://fonts.gstatic.com/s/lato/v13/1YwB1sO8YE1Lyjf12WNiUA.woff2"

$(TMP_WEB_PATH)/lato-v11-latin-700.woff2: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://fonts.gstatic.com/s/lato/v13/H2DMvhDLycM56KNuAtbJYA.woff2"

$(TMP_WEB_PATH)/lato-v11-latin-900.woff2: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://fonts.gstatic.com/s/lato/v13/tI4j516nok_GrVf4dhunkg.woff2"

$(TMP_WEB_PATH)/html-imports.min.js: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://raw.githubusercontent.com/webcomponents/html-imports/master/html-imports.min.js"

$(TMP_WEB_PATH)/html-imports.min.js.map: $(WEB_ASSET_DOWNLOADER)
	mkdir -p $(@D)
	bash "$(WEB_ASSET_DOWNLOADER)" "$@" "https://raw.githubusercontent.com/webcomponents/html-imports/master/html-imports.min.js.map"

$(TMP_WEB_PATH)/navigation.html: $(WEB_PATH)/navigation.html
	mkdir -p $(@D)
	cp $< $@
