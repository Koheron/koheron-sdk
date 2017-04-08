
WEB_DOWNLOADS := $(TMP_WEB_PATH)/_koheron.css
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.resize.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.selection.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.time.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.flot.axislabels.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/bootstrap.min.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/bootstrap.min.css
WEB_DOWNLOADS += $(TMP_WEB_PATH)/jquery.min.js
WEB_DOWNLOADS += $(TMP_WEB_PATH)/_koheron.svg
WEB_DOWNLOADS += $(TMP_WEB_PATH)/_koheron_logo.svg
WEB_DOWNLOADS += $(TMP_WEB_PATH)/kbird.ico
WEB_DOWNLOADS += $(TMP_WEB_PATH)/lato-v11-latin-400.woff2
WEB_DOWNLOADS += $(TMP_WEB_PATH)/lato-v11-latin-700.woff2
WEB_DOWNLOADS += $(TMP_WEB_PATH)/lato-v11-latin-900.woff2
WEB_DOWNLOADS += $(TMP_WEB_PATH)/glyphicons-halflings-regular.woff2

$(TMP_WEB_PATH)/_koheron.css:
	mkdir -p $(@D)
	curl https://www.koheron.com/static/css/main.css -o $@

$(TMP_WEB_PATH)/jquery.flot.js:
	mkdir -p $(@D)
	curl https://cdnjs.cloudflare.com/ajax/libs/flot/0.8.3/jquery.flot.min.js -o $@

$(TMP_WEB_PATH)/jquery.flot.resize.js:
	mkdir -p $(@D)
	curl https://cdnjs.cloudflare.com/ajax/libs/flot/0.8.3/jquery.flot.resize.min.js -o $@

$(TMP_WEB_PATH)/jquery.flot.selection.js:
	mkdir -p $(@D)
	curl https://cdnjs.cloudflare.com/ajax/libs/flot/0.8.3/jquery.flot.selection.min.js -o $@

$(TMP_WEB_PATH)/jquery.flot.time.js:
	mkdir -p $(@D)
	curl https://cdnjs.cloudflare.com/ajax/libs/flot/0.8.3/jquery.flot.time.min.js -o $@

$(TMP_WEB_PATH)/jquery.flot.axislabels.js:
	mkdir -p $(@D)
	curl https://raw.githubusercontent.com/markrcote/flot-axislabels/master/jquery.flot.axislabels.js -o $@

$(TMP_WEB_PATH)/bootstrap.min.js:
	mkdir -p $(@D)
	curl http://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js -o $@

$(TMP_WEB_PATH)/bootstrap.min.css:
	mkdir -p $(@D)
	curl http://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css -o $@

$(TMP_WEB_PATH)/jquery.min.js:
	mkdir -p $(@D)
	curl https://code.jquery.com/jquery-3.2.0.min.js -o $@

$(TMP_WEB_PATH)/_koheron.svg:
	mkdir -p $(@D)
	curl https://www.koheron.com/static/images/website/koheron.svg -o $@

$(TMP_WEB_PATH)/_koheron_logo.svg:
	mkdir -p $(@D)
	curl https://www.koheron.com/static/images/website/koheron_logo.svg -o $@

$(TMP_WEB_PATH)/kbird.ico:
	mkdir -p $(@D)
	curl https://www.koheron.com/static/images/website/kbird.ico -o $@

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