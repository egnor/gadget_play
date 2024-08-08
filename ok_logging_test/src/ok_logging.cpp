#include "ok_logging.h"

#include <cstdlib>
#include <cstring>

#include <Arduino.h>

static Print* log_output = &Serial;

static OkLoggingLevel level_for_tag(char const*);

OkLoggingContext::OkLoggingContext(char const* tag)
  : tag(tag), level(level_for_tag(tag)) {}

void set_ok_logging_output(Print* output) { log_output = output; }

void ok_log(char const* tag, OkLoggingLevel lev, char const* f, ...) {
  if (log_output == nullptr) return;

  log_output->print(millis() * 1e-3f, 3);

  switch (lev) {
    case OK_FATAL_LEVEL: log_output->print(" 💥 "); break;
    case OK_PROBLEM_LEVEL: log_output->print(" 🔥 "); break;
    case OK_NOTE_LEVEL: log_output->print(" "); break;
    case OK_SPAM_LEVEL: log_output->print(" 🕸️ "); break;
  }

  if (tag != nullptr && tag[0] != '\0') {
    log_output->print("[");
    log_output->print(tag);
    log_output->print("] ");
  }

  if (lev == OK_FATAL_LEVEL) log_output->print("FATAL ");

  char stack_buf[128];
  char* buf = stack_buf;
  int buf_size = sizeof(stack_buf);
  while (true) {
    va_list args;
    va_start(args, f);
    auto len = vsnprintf(buf, buf_size, f, args);
    va_end(args);

    if (len >= buf_size) {
      if (buf != stack_buf) delete[] buf;
      buf_size = len + 1;
      buf = new char[buf_size];
    } else {
      if (len < 0) strncpy(buf, "[log message formatting error]", buf_size);
      while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) --len;
      break;
    }
  }

  log_output->println(buf);
  if (buf != stack_buf) delete[] buf;
  if (lev == OK_FATAL_LEVEL) {
    log_output->println("  🚨 REBOOT IN 1 SEC 🚨\n");
    log_output->flush();
    delay(1000);
    abort();
  }
}

static char const* next_of(char const* p, char const* end, char const* m) {
  while (p < end && !strchr(m, *p)) ++p;
  return p;
}

static bool glob_match(
    char const* glob, char const* glob_end,
    char const* match, char const* match_end) {
  // Text before the first '*' must match exactly
  char const* next_star = next_of(glob, glob_end, "*");
  if (strncasecmp(match, glob, next_star - glob)) return false;
  if (next_star >= glob_end) return true;

  // Text between '*'s must match in order
  while (true) {
    match += (next_star - glob);
    glob = next_star + 1;
    next_star = next_of(glob, glob_end, "*");
    if (next_star >= glob_end) break;

    char const* last_match = match_end - (next_star - glob);
    while (strncasecmp(match, glob, next_star - glob)) {
      if (++match > last_match) return false;
    }
  }

  // Text after the last '*' must be a suffix
  return 
      match_end - match >= glob_end - glob &&
      strncasecmp(match_end - (glob_end - glob), glob, glob_end - glob) == 0;
}

static void trim(char const** p, char const** end) {
  while (*p < *end && strchr(" \t\n", **p)) ++*p;
  while (*p < *end && strchr(" \t\n", (*end)[-1])) --*end;
}

static OkLoggingLevel level_for_name(char const* level, char const* end) {
  auto const is = [level, end](char const* s) {
    return !strncasecmp(level, s, end - level) && !s[end - level];
  };

  if (is("*") ||
      is("a") || is("all") ||
      is("d") || is("debug") ||
      is("s") || is("spam") ||
      is("v") || is("verbose")) {
    return OK_SPAM_LEVEL;
  }

  if (is("default") ||
      is("i") || is("info") ||
      is("n") || is("normal") || is("note") || is("notice") || is("notable")) {
    return OK_NOTE_LEVEL;
  }

  if (is("e") || is("error") ||
      is("p") || is("prob") || is("problem") ||
      is("w") || is("warn") || is("warning")) {
    return OK_PROBLEM_LEVEL;
  }

  if (is("none") || is("f") || is("fatal") || is("p") || is("panic")) {
    return OK_FATAL_LEVEL;
  }

  ok_log(
      "ok_logging", OK_PROBLEM_LEVEL, "Bad level \"%.*s\" in config:\n  %s",
      end - level, level, ok_logging_config);
  return OK_SPAM_LEVEL;
}

static OkLoggingLevel level_for_tag(char const* tag) {
  if (ok_logging_config == nullptr) return OK_NOTE_LEVEL;

  // Find the "tag=level,..." entry that matches the given tag
  char const* config_end = ok_logging_config + strlen(ok_logging_config);
  char const* tag_end = tag + strlen(tag);
  char const* pos = ok_logging_config;
  while (true) {
    char const* entry = pos;
    char const* entry_end = next_of(pos, config_end, ",;");

    char const* glob = entry;
    char const* glob_end = next_of(entry, entry_end, "=:");

    char const* level = glob_end < entry_end ? glob_end + 1 : glob_end;
    char const* level_end = entry_end;

    trim(&glob, &glob_end);
    trim(&level, &level_end);
    if (glob == glob_end || level == level_end) {
      ok_log(
          "ok_logging", OK_PROBLEM_LEVEL, "Bad entry \"%.*s\" in config:\n  %s",
          level_end - glob, glob, ok_logging_config);
    } else if (glob_match(glob, glob_end, tag, tag_end)) {
      return level_for_name(level, level_end);
    }

    if (entry_end == config_end) return OK_NOTE_LEVEL;  // Default
    pos = entry_end + 1;
  }
}
