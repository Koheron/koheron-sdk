# Shared RPC adapter and event bindings; board templates are supplied by callers.
include $(SDK_PATH)/web/clock-generator/driver.mk
include $(SDK_PATH)/web/instrument/events.mk
WEB_FILES += $(SDK_PATH)/web/clock-generator/clock-generator-app.ts
