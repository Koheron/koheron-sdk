#pragma once
#include <cstdio>
enum Severity {INFO, ERROR};
template<Severity severity = INFO, typename... Args> void logf(const char*, Args...) {}
template<Severity severity, typename... Args> void log(const char*, Args...) {}
