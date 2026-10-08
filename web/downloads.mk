
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

# Owned Canvas 2D plotting stack, built from readable local sources.
PLOT_BUILD ?= $(WEB_DOCKER_RUN) node $(WEB_PATH)/plotting/build.cjs
PLOT_ASSETS := $(addprefix $(TMP_WEB_PATH)/,jquery.flot.js jquery.flot.resize.js jquery.flot.selection.js jquery.flot.time.js jquery.flot.axislabels.js jquery.flot.canvas.js flot-LICENSE.txt)
WEB_DOWNLOADS += $(TMP_WEB_PATH)/flot-LICENSE.txt
$(PLOT_ASSETS) &: $(wildcard $(WEB_PATH)/plotting/src/*.js) $(WEB_PATH)/plotting/build.cjs $(WEB_PATH)/plotting/licenses/LICENSE.txt $(WEB_PATH)/plotting/package.json $(WEB_PATH)/plotting/package-lock.json
	$(PLOT_BUILD) $(TMP_WEB_PATH)

$(TMP_WEB_PATH)/_koheron.css:
	mkdir -p $(@D)
	curl https://assets.koheron.com/css/main.css -o $@

$(TMP_WEB_PATH)/bootstrap.min.js:
	mkdir -p $(@D)
	curl https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js -o $@

$(TMP_WEB_PATH)/bootstrap.min.css:
	mkdir -p $(@D)
	curl https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css -o $@

$(TMP_WEB_PATH)/jquery.min.js:
	mkdir -p $(@D)
	curl https://code.jquery.com/jquery-3.2.0.min.js -o $@

$(TMP_WEB_PATH)/_koheron.png:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron.png -o $@

$(TMP_WEB_PATH)/_koheron_logo.svg:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron_logo.svg -o $@

$(TMP_WEB_PATH)/kbird.ico:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron.ico -o $@

$(TMP_WEB_PATH)/lato-v11-latin-400.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/1YwB1sO8YE1Lyjf12WNiUA.woff2 -o $@

$(TMP_WEB_PATH)/lato-v11-latin-700.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/H2DMvhDLycM56KNuAtbJYA.woff2 -o $@

$(TMP_WEB_PATH)/lato-v11-latin-900.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/tI4j516nok_GrVf4dhunkg.woff2 -o $@

$(TMP_WEB_PATH)/glyphicons-halflings-regular.woff2:
	mkdir -p $(@D)
	curl https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/fonts/glyphicons-halflings-regular.woff2 -o $@

$(TMP_WEB_PATH)/html-imports.min.js:
	mkdir -p $(@D)
	curl https://raw.githubusercontent.com/webcomponents/html-imports/master/html-imports.min.js -o $@

$(TMP_WEB_PATH)/html-imports.min.js.map:
	mkdir -p $(@D)
	curl https://raw.githubusercontent.com/webcomponents/html-imports/master/html-imports.min.js.map -o $@

$(TMP_WEB_PATH)/navigation.html: $(WEB_PATH)/navigation.html
	mkdir -p $(@D)
	cp $< $@
