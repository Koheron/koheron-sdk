# Local web sources follow shared components; the application starts last.
PROJECT_WEB_FILES := $(shell find "$(PROJECT_PATH)/web" -type f \( -name '*.ts' -o -name '*.html' -o -name '*.css' \))
WEB_FILES += $(filter-out $(PROJECT_PATH)/web/app.ts,$(PROJECT_WEB_FILES))
WEB_FILES += $(wildcard $(PROJECT_PATH)/web/app.ts)
