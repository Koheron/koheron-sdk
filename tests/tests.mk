
PHONY: tests
tests: tests_py tests_js

PHONY: tests_py
tests_py: run
	HOST=$(HOST) pytest -v $(TESTS_PATH)/tests.py

# TODO fix ugly hack
$(TMP)/koheron_with_exports.ts: $(WEB_PATH)/koheron.ts
	rm -f $@
	cat $(TESTS_PATH)/export_hack $(WEB_PATH)/koheron.ts >> $@

$(TESTS_PATH)/tests.js: $(TESTS_PATH)/tests.ts $(TMP)/koheron_with_exports.ts
	tsc $< --module commonjs

PHONY: test_js
tests_js: run $(TESTS_PATH)/tests.js
	HOST=$(HOST) nodeunit $(TESTS_PATH)/tests.js
