# Include after selecting OBJ, GEN_HEADERS and any PCH flags/prerequisites.
SERVER_FLAGS_STAMP := $(TMP_SERVER_PATH)/.compile-settings
$(eval $(call build_settings_stamp,$(SERVER_FLAGS_STAMP),$$(call build_shell_quote,$$(SERVER_CCXX)) $$(call build_shell_quote,$$(SERVER_CCXXFLAGS))))
SERVER_LINK_STAMP := $(TMP_SERVER_PATH)/.link-settings
$(eval $(call build_settings_stamp,$(SERVER_LINK_STAMP),$$(call build_shell_quote,$$(cmd_link))))

DEP := $(OBJ:.o=.d)
-include $(DEP)

# Generated headers must exist before the first compilation; compiler dependency
# files then rebuild only their consumers when the header contents change.
$(OBJ): $(SERVER_COMPILER_STAMP) $(SERVER_FLAGS_STAMP) | $(GEN_HEADERS)

ifneq ($(APP_PATH),)
$(TMP_SERVER_PATH)/%.o: $(APP_PATH)/%.cpp
	@mkdir -p $(dir $@)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)
endif

define server_compile_rule
$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/$(1)/%.cpp
	@mkdir -p $$(dir $$@)
	$$(call echo-cmd,cxx)
	$$(Q)$$(call cmd,cmd_cxx)
endef
$(foreach directory,network executor runtime hardware utilities main,$(eval $(call server_compile_rule,$(directory))))

$(TMP_SERVER_PATH)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(call echo-cmd,cxx)
	$(Q)$(call cmd,cmd_cxx)

$(SERVER): $(OBJ) $(SERVER_TEMPLATE_LIST) $(GEN_HEADERS) $(SERVER_FLAGS_STAMP) $(SERVER_LINK_STAMP)
	@mkdir -p $(dir $@)
	$(call echo-cmd,link)
	$(Q)$(call cmd,cmd_link)
	$(call ok,$@)

.PHONY: clean_server
clean_server:
	rm -rf $(TMP_SERVER_PATH)
