#include <Arduino.h>

#include "src/ok_logging.h"

extern char const* const ok_logging_config =
    " prefix*:SpAm;*suffix:warn ,*f*r*i*e*n*d*s* = INFO;"
    "xyzzy;=xyzzy,xyzzy=, * : none ";

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.printf("=== TEST ===\nConfig: [%s]\n", ok_logging_config);
  {
    OkLoggingContext OK_CONTEXT("suffix-not-prefix");
    Serial.printf(
        "\n[%s] level=%d expect=%d\n",
        OK_CONTEXT.tag, OK_CONTEXT.level, OK_FATAL_LEVEL);
    OK_SPAM("spam");
    OK_NOTE("note");
    OK_PROBLEM("problem");
  }
  {
    OkLoggingContext OK_CONTEXT("prefix-with-stuff");
    Serial.printf(
        "\n[%s] level=%d expect=%d\n",
        OK_CONTEXT.tag, OK_CONTEXT.level, OK_SPAM_LEVEL);
    OK_SPAM("spam");
    OK_NOTE("note");
    OK_PROBLEM("problem");
  }
  {
    OkLoggingContext OK_CONTEXT("prefix");
    Serial.printf(
        "\n[%s] level=%d expect=%d\n",
        OK_CONTEXT.tag, OK_CONTEXT.level, OK_SPAM_LEVEL);
    OK_SPAM("spam");
    OK_NOTE("note");
    OK_PROBLEM("problem");
  }
  {
    OkLoggingContext OK_CONTEXT("suffix");
    Serial.printf(
        "\n[%s] level=%d expect=%d\n",
        OK_CONTEXT.tag, OK_CONTEXT.level, OK_PROBLEM_LEVEL);
    OK_SPAM("spam");
    OK_NOTE("note");
    OK_PROBLEM("problem");
  }
  {
    OkLoggingContext OK_CONTEXT("stuff-with-suffix");
    Serial.printf(
        "\n[%s] level=%d expect=%d\n",
        OK_CONTEXT.tag, OK_CONTEXT.level, OK_PROBLEM_LEVEL);
    OK_SPAM("spam");
    OK_NOTE("note");
    OK_PROBLEM("problem");
  }
  {
    OkLoggingContext OK_CONTEXT("friends");
    Serial.printf(
        "\n[%s] level=%d expect=%d\n",
        OK_CONTEXT.tag, OK_CONTEXT.level, OK_NOTE_LEVEL);
    OK_SPAM("spam");
    OK_NOTE("note");
    OK_PROBLEM("problem");
  }
  {
    OkLoggingContext OK_CONTEXT("{four/five seconds}");
    Serial.printf(
        "\n[%s] level=%d expect=%d\n",
        OK_CONTEXT.tag, OK_CONTEXT.level, OK_NOTE_LEVEL);
    OK_SPAM("spam");
    OK_NOTE("note");
    OK_PROBLEM("problem");
  }
  Serial.println();
  delay(1000);
}
