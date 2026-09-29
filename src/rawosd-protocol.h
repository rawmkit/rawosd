#ifndef RAWOSD_PROTOCOL_H
#define RAWOSD_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define RAWOSD_LAYOUT_NAME_MAX 32u

typedef enum {
  RAWOSD_FEEDBACK_MONITOR,
  RAWOSD_FEEDBACK_MASTER_COUNT,
  RAWOSD_FEEDBACK_MASTER_FACTOR,
  RAWOSD_FEEDBACK_MOVE,
  RAWOSD_FEEDBACK_RESIZE,
  RAWOSD_FEEDBACK_VIEW,
  RAWOSD_FEEDBACK_LAYOUT,
  RAWOSD_FEEDBACK_FLOATING,
  RAWOSD_FEEDBACK_FULLSCREEN,
  RAWOSD_FEEDBACK_TAG,
  RAWOSD_FEEDBACK_TOGGLE_TAG,
  RAWOSD_FEEDBACK_TAG_MONITOR,
  RAWOSD_FEEDBACK_EVENT_LAST
} RawosdFeedbackEvent;

typedef struct {
  uint64_t serial;
  RawosdFeedbackEvent event;
  int monitor;
  int32_t arg0;
  int32_t arg1;
} RawosdFeedback;

typedef struct {
  int number;
  int selected;
  int x;
  int y;
  int width;
  int height;
  uint32_t viewed;
  uint32_t occupied;
  uint32_t urgent;
  char layout[RAWOSD_LAYOUT_NAME_MAX];
  unsigned long focused;
  int nmaster;
  double mfact;
  unsigned int focused_index;
  unsigned int visible_count;
  char **tags;
} RawosdMonitorState;

typedef struct {
  size_t tag_count;
  int selected_monitor;
  RawosdMonitorState *monitors;
  size_t monitor_count;
} RawosdState;

int rawosd_feedback_parse(const char *record, RawosdFeedback *feedback,
                          FILE *err);
const char *rawosd_feedback_event_name(RawosdFeedbackEvent event);
int rawosd_state_parse(const char *snapshot, RawosdState *state, FILE *err);
void rawosd_state_destroy(RawosdState *state);
const RawosdMonitorState *rawosd_state_monitor(const RawosdState *state,
                                               int number);

#endif
