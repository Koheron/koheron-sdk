#pragma once
enum Severity {INFO, WARNING, ERROR};
template<Severity severity = INFO, class... Args> void logf(const char*, Args&&...) {}
template<Severity severity, class... Args> void log(const char*, Args&&...) {}
