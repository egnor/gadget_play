// Simple, robust printf logging for Arduino-ish code.

#pragma once

namespace arduino { class Print; }

enum OkLoggingLevel {
  OK_SPAM_LEVEL,
  OK_NOTE_LEVEL,
  OK_PROBLEM_LEVEL,
  OK_FATAL_LEVEL,
};

// Logging macros expect an OkLoggingContext named OK_CONTEXT in scope
struct OkLoggingContext {
  char const* tag;
  OkLoggingLevel level;
  OkLoggingContext(char const* tag);  // Initializes tag and level
};

// Define this global constant as a comma-separated list of "tag=level"
// settings with "*" wildcards, eg. "mymodule=spam,subsystem.*=problem,*=note".
extern char const* const ok_logging_config;

// TODO: add numeric spam levels with contextual adjustment?
#define OK_SPAM(fmt, ...) OK_REPORT(OK_SPAM_LEVEL, fmt, ##__VA_ARGS__)
#define OK_NOTE(fmt, ...) OK_REPORT(OK_NOTE_LEVEL, fmt, ##__VA_ARGS__)
#define OK_PROBLEM(fmt, ...) OK_REPORT(OK_PROBLEM_LEVEL, fmt, ##__VA_ARGS__)
#define OK_FATAL(fmt, ...) for(;;) OK_REPORT(OK_FATAL_LEVEL, fmt, ##__VA_ARGS__)
#define OK_ASSERT(c) if (c) {} else OK_FATAL( \
    "Assertion failed: %s\n  %s:%d\n  %s", \
    #c, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define OK_REPORT(l, f, ...) if (l >= OK_CONTEXT.level) \
    ok_log(OK_CONTEXT.tag, l, (f), ##__VA_ARGS__); else {}

void set_ok_logging_output(arduino::Print*);
void ok_log(char const* tag, OkLoggingLevel, char const* fmt, ...);
