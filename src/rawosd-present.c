#include "rawosd-present.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int
append_text(char *buffer, size_t capacity, size_t *used, const char *text)
{
  int n;

  if (*used >= capacity)
    return -1;
  n = snprintf(buffer + *used, capacity - *used, "%s", text);
  if (n < 0 || (size_t)n >= capacity - *used)
    return -1;
  *used += (size_t)n;
  return 0;
}

static int
format_tags(const RawosdState *state, const RawosdMonitorState *monitor,
            uint32_t mask, char *buffer, size_t capacity)
{
  size_t i;
  size_t used = 0;
  bool first = true;
  uint32_t all;

  if (!state || !monitor || capacity == 0)
    return -1;
  buffer[0] = '\0';
  all = state->tag_count == 31 ? 0x7fffffffu
                               : ((1u << state->tag_count) - 1u);
  mask &= all;
  if (mask == all)
    return append_text(buffer, capacity, &used, "all");
  for (i = 0; i < state->tag_count; ++i)
  {
    if (!(mask & (1u << i)))
      continue;
    if (!first && append_text(buffer, capacity, &used, "+") < 0)
      return -1;
    if (append_text(buffer, capacity, &used, monitor->tags[i]) < 0)
      return -1;
    first = false;
  }
  if (first)
    return append_text(buffer, capacity, &used, "none");
  return 0;
}

static const char *
layout_symbol(const RawosdConfig *config, int id)
{
  if (id < 0 || id >= 6)
    return NULL;
  return config->layout_symbols[id];
}

static int
layout_id(const char *name)
{
  static const char *const names[6] = {
    "tile", "floating", "monocle", "bstack", "bstack_horizontal", "grid",
  };
  int i;

  for (i = 0; i < 6; ++i)
    if (strcmp(name, names[i]) == 0)
      return i;
  return -1;
}

int
rawosd_present_format(const RawosdFeedback *feedback,
                      const RawosdState *state,
                      const RawosdConfig *config,
                      RawosdPresentation *presentation,
                      FILE *err)
{
  const RawosdMonitorState *monitor;
  char tags[256];
  const char *symbol;
  int n = -1;

  if (!feedback || !state || !config || !presentation)
    return -1;
  memset(presentation, 0, sizeof(*presentation));
  presentation->monitor = feedback->monitor;
  presentation->scheme = RAWOSD_SCHEME_NORMAL;
  monitor = rawosd_state_monitor(state, feedback->monitor);
  if (!monitor)
  {
    if (err)
      fprintf(err, "rawosd: feedback references unknown monitor %d\n",
              feedback->monitor);
    return -1;
  }

  switch (feedback->event)
  {
    case RAWOSD_FEEDBACK_MONITOR:
      if (format_tags(state, monitor, monitor->viewed, tags, sizeof(tags)) < 0)
        return -1;
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "monitor %d  ·  %s", feedback->arg0, tags);
      break;
    case RAWOSD_FEEDBACK_MASTER_COUNT:
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "master %d", feedback->arg0);
      break;
    case RAWOSD_FEEDBACK_MASTER_FACTOR:
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "master %d%%", (feedback->arg0 + 5000) / 10000);
      break;
    case RAWOSD_FEEDBACK_MOVE:
      if (!config->geometry)
        return 0;
      presentation->scheme = RAWOSD_SCHEME_FLOATING;
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "x %d  ·  y %d", feedback->arg0, feedback->arg1);
      break;
    case RAWOSD_FEEDBACK_RESIZE:
      if (!config->geometry)
        return 0;
      presentation->scheme = RAWOSD_SCHEME_FLOATING;
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "%d × %d", feedback->arg0, feedback->arg1);
      break;
    case RAWOSD_FEEDBACK_VIEW:
      if (format_tags(state, monitor, (uint32_t)feedback->arg0,
                      tags, sizeof(tags)) < 0)
        return -1;
      symbol = layout_symbol(config, layout_id(monitor->layout));
      if (!symbol)
        return -1;
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "%s  ·  %s", tags, symbol);
      break;
    case RAWOSD_FEEDBACK_LAYOUT:
      symbol = layout_symbol(config, feedback->arg0);
      if (!symbol)
        return -1;
      n = snprintf(presentation->text, sizeof(presentation->text), "%s",
                   symbol);
      break;
    case RAWOSD_FEEDBACK_FLOATING:
      presentation->scheme = feedback->arg0 ? RAWOSD_SCHEME_FLOATING
                                             : RAWOSD_SCHEME_NORMAL;
      n = snprintf(presentation->text, sizeof(presentation->text), "%s",
                   feedback->arg0 ? "float" : "tile");
      break;
    case RAWOSD_FEEDBACK_FULLSCREEN:
      n = snprintf(presentation->text, sizeof(presentation->text), "%s",
                   feedback->arg0 ? "fullscreen" : "windowed");
      break;
    case RAWOSD_FEEDBACK_TAG:
      if (format_tags(state, monitor, (uint32_t)feedback->arg0,
                      tags, sizeof(tags)) < 0)
        return -1;
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "send  ·  %s", tags);
      break;
    case RAWOSD_FEEDBACK_TOGGLE_TAG:
      if (format_tags(state, monitor, (uint32_t)feedback->arg0,
                      tags, sizeof(tags)) < 0)
        return -1;
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "tags  ·  %s", tags);
      break;
    case RAWOSD_FEEDBACK_TAG_MONITOR:
      n = snprintf(presentation->text, sizeof(presentation->text),
                   "send  ·  monitor %d", feedback->arg0);
      break;
    case RAWOSD_FEEDBACK_EVENT_LAST:
      return -1;
  }

  if (n < 0 || (size_t)n >= sizeof(presentation->text))
    return -1;
  presentation->show = true;
  return 0;
}
