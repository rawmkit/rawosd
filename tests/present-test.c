#include "rawosd-config.h"
#include "rawosd-present.h"
#include "rawosd-protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char snapshot[] =
  "rawm-state-v2\n"
  "tag-count\t3\n"
  "selected-monitor\t0\n"
  "monitor\t0\t1\t0\t0\t1920\t1080\t0\t24\t1920\t1056\t00000003\t00000003\t00000000\ttile\t0x1\t1\t0.550000\n"
  "stack\t0\t1\t1\n"
  "tag\t0\t1\twork\n"
  "tag\t0\t2\tweb\n"
  "tag\t0\t3\tchat\n";

static void
expect(int condition, const char *message)
{
  if (!condition)
  {
    fprintf(stderr, "present test failed: %s\n", message);
    exit(EXIT_FAILURE);
  }
}

static void
format_expect(RawosdState *state, RawosdConfig *config,
              RawosdFeedbackEvent event, int arg0, int arg1,
              const char *expected)
{
  RawosdFeedback feedback = {
    .serial = 1,
    .event = event,
    .monitor = 0,
    .arg0 = arg0,
    .arg1 = arg1,
  };
  RawosdPresentation presentation;

  expect(rawosd_present_format(&feedback, state, config, &presentation,
                               stderr) == 0,
         "format failed");
  expect(presentation.show, "presentation unexpectedly suppressed");
  expect(strcmp(presentation.text, expected) == 0, "formatted text differs");
}

int
main(void)
{
  RawosdConfig config;
  RawosdState state = { .selected_monitor = -1 };
  RawosdFeedback feedback = {
    .serial = 2,
    .event = RAWOSD_FEEDBACK_MOVE,
    .monitor = 0,
    .arg0 = 11,
    .arg1 = 22,
  };
  RawosdPresentation presentation;

  rawosd_config_init(&config);
  expect(rawosd_state_parse(snapshot, &state, stderr) == 0,
         "state setup failed");
  format_expect(&state, &config, RAWOSD_FEEDBACK_MONITOR, 0, 0,
                "monitor 0  ·  work+web");
  format_expect(&state, &config, RAWOSD_FEEDBACK_MASTER_COUNT, 2, 0,
                "master 2");
  format_expect(&state, &config, RAWOSD_FEEDBACK_MASTER_FACTOR, 555000, 0,
                "master 56%");
  format_expect(&state, &config, RAWOSD_FEEDBACK_VIEW, 3, 0,
                "work+web  ·  []=");
  format_expect(&state, &config, RAWOSD_FEEDBACK_LAYOUT, 5, 0, "###");
  format_expect(&state, &config, RAWOSD_FEEDBACK_FLOATING, 1, 0, "float");
  format_expect(&state, &config, RAWOSD_FEEDBACK_FULLSCREEN, 0, 0, "windowed");
  format_expect(&state, &config, RAWOSD_FEEDBACK_TAG, 4, 0,
                "send  ·  chat");
  format_expect(&state, &config, RAWOSD_FEEDBACK_TOGGLE_TAG, 5, 0,
                "tags  ·  work+chat");
  format_expect(&state, &config, RAWOSD_FEEDBACK_TAG_MONITOR, 2, 0,
                "send  ·  monitor 2");

  expect(rawosd_present_format(&feedback, &state, &config, &presentation,
                               stderr) == 0 && !presentation.show,
         "geometry event not suppressed by default");
  config.geometry = true;
  expect(rawosd_present_format(&feedback, &state, &config, &presentation,
                               stderr) == 0 && presentation.show &&
         strcmp(presentation.text, "x 11  ·  y 22") == 0 &&
         presentation.scheme == RAWOSD_SCHEME_FLOATING,
         "geometry presentation differs");

  {
    RawosdFeedback selected_feedback = {
      .serial = 3, .event = RAWOSD_FEEDBACK_LAYOUT, .monitor = 0,
      .arg0 = 0, .arg1 = 0
    };
    expect(rawosd_present_format(&selected_feedback, &state, &config,
                                 &presentation, stderr) == 0 &&
           presentation.scheme == RAWOSD_SCHEME_SELECTED,
           "ordinary feedback did not use selected scheme");
  }

  rawosd_state_destroy(&state);
  rawosd_config_destroy(&config);
  puts("presentation tests: ok");
  return EXIT_SUCCESS;
}
