#include "rawosd-protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
expect(int condition, const char *message)
{
  if (!condition)
  {
    fprintf(stderr, "protocol test failed: %s\n", message);
    exit(EXIT_FAILURE);
  }
}

static void
test_feedback(void)
{
  RawosdFeedback feedback;

  expect(rawosd_feedback_parse(
           "rawm-feedback-v1\t42\tview\t1\t5\t0\n",
           &feedback, stderr) == 0,
         "valid feedback rejected");
  expect(feedback.serial == 42 && feedback.event == RAWOSD_FEEDBACK_VIEW &&
         feedback.monitor == 1 && feedback.arg0 == 5 && feedback.arg1 == 0,
         "feedback fields differ");
  expect(strcmp(rawosd_feedback_event_name(feedback.event), "view") == 0,
         "feedback name differs");

  expect(rawosd_feedback_parse(
           "rawm-feedback-v2\t42\tview\t1\t5\t0",
           &feedback, NULL) < 0,
         "unsupported feedback version accepted");
  expect(rawosd_feedback_parse(
           "rawm-feedback-v1\t42\tunknown\t1\t5\t0",
           &feedback, NULL) < 0,
         "unknown feedback event accepted");
  expect(rawosd_feedback_parse(
           "rawm-feedback-v1\t42\tview\t1\t5",
           &feedback, NULL) < 0,
         "short feedback accepted");
  expect(rawosd_feedback_parse(
           "rawm-feedback-v1\t42\tview\t1\t5\t0\textra",
           &feedback, NULL) < 0,
         "long feedback accepted");
  expect(rawosd_feedback_parse(
           "rawm-feedback-v1\t0\tlayout\t0\t1\t0",
           &feedback, NULL) < 0,
         "zero feedback serial accepted");
  expect(rawosd_feedback_parse(
           "rawm-feedback-v1\t43\tlayout\t0\t6\t0",
           &feedback, NULL) < 0,
         "invalid layout id accepted");
}

static void
test_state(void)
{
  static const char snapshot[] =
    "rawm-state-v1\n"
    "tag-count\t3\n"
    "selected-monitor\t1\n"
    "monitor\t0\t0\t0\t0\t1920\t1080\t00000001\t00000003\t00000000\ttile\t0x0\t1\t0.550000\n"
    "stack\t0\t0\t2\n"
    "tag\t0\t1\t1\n"
    "tag\t0\t2\tweb%25dev\n"
    "tag\t0\t3\tchat\n"
    "monitor\t1\t1\t1920\t0\t1920\t1080\t00000006\t00000006\t00000004\tgrid\t0x1234\t2\t0.600000\n"
    "stack\t1\t1\t1\n"
    "tag\t1\t1\tone\n"
    "tag\t1\t2\ttwo\n"
    "tag\t1\t3\tthree%09x\n";
  RawosdState state = { .selected_monitor = -1 };
  const RawosdMonitorState *monitor;

  expect(rawosd_state_parse(snapshot, &state, stderr) == 0,
         "valid state rejected");
  expect(state.tag_count == 3 && state.monitor_count == 2 &&
         state.selected_monitor == 1,
         "state header differs");
  monitor = rawosd_state_monitor(&state, 1);
  expect(monitor != NULL && monitor->selected == 1 &&
         monitor->x == 1920 && monitor->width == 1920 &&
         monitor->viewed == 0x6 && strcmp(monitor->layout, "grid") == 0 &&
         monitor->focused == 0x1234 && monitor->nmaster == 2 &&
         monitor->focused_index == 1 && monitor->visible_count == 1,
         "monitor fields differ");
  expect(strcmp(monitor->tags[2], "three\tx") == 0,
         "tag decoding differs");
  monitor = rawosd_state_monitor(&state, 0);
  expect(monitor != NULL && strcmp(monitor->tags[1], "web%dev") == 0,
         "percent decoding differs");
  rawosd_state_destroy(&state);

  expect(rawosd_state_parse(
           "rawm-state-v1\n\ntag-count\t1\nselected-monitor\t-1\n",
           &state, NULL) < 0,
         "blank state record accepted");
  expect(rawosd_state_parse(
           "rawm-state-v2\ntag-count\t1\nselected-monitor\t-1\n",
           &state, NULL) < 0,
         "unsupported state version accepted");
  expect(rawosd_state_parse(
           "rawm-state-v1\ntag-count\t1\nselected-monitor\t0\n"
           "unknown\t0\n",
           &state, NULL) < 0,
         "unknown state record accepted");
  expect(rawosd_state_parse(
           "rawm-state-v1\ntag-count\t1\nselected-monitor\t0\n"
           "monitor\t0\t1\t0\t0\t800\t600\t00000001\t00000001\t00000000\ttile\t0x0\t1\t0.550000\n"
           "tag\t0\t1\t1\n",
           &state, NULL) < 0,
         "state without stack record accepted");
  expect(rawosd_state_parse(
           "rawm-state-v1\ntag-count\t1\nselected-monitor\t0\n"
           "monitor\t0\t1\t0\t0\t800\t600\t+0000001\t00000001\t00000000\ttile\t0x0\t1\t0.550000\n"
           "stack\t0\t0\t0\n"
           "tag\t0\t1\t1\n",
           &state, NULL) < 0,
         "non-canonical tag mask accepted");
  rawosd_state_destroy(&state);
}

int
main(void)
{
  test_feedback();
  test_state();
  puts("protocol tests: ok");
  return EXIT_SUCCESS;
}
