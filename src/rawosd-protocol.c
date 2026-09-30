#include "rawosd-protocol.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int
fail(FILE *err, const char *message)
{
  if (err)
    fprintf(err, "rawosd: %s\n", message);
  return -1;
}

static int
parse_i32(const char *text, int32_t *value)
{
  char *end = NULL;
  long parsed;

  if (!text || !*text)
    return -1;
  errno = 0;
  parsed = strtol(text, &end, 10);
  if (errno || !end || *end || parsed < INT32_MIN || parsed > INT32_MAX)
    return -1;
  *value = (int32_t)parsed;
  return 0;
}

static int
parse_int(const char *text, int *value)
{
  int32_t parsed;

  if (parse_i32(text, &parsed) < 0)
    return -1;
  *value = (int)parsed;
  return 0;
}

static int
parse_u32_hex(const char *text, uint32_t *value)
{
  char *end = NULL;
  unsigned long parsed;
  size_t i;

  if (!text || strlen(text) != 8u)
    return -1;
  for (i = 0; i < 8u; ++i)
    if (!((text[i] >= '0' && text[i] <= '9') ||
          (text[i] >= 'a' && text[i] <= 'f') ||
          (text[i] >= 'A' && text[i] <= 'F')))
      return -1;
  errno = 0;
  parsed = strtoul(text, &end, 16);
  if (errno || !end || end == text || *end || parsed > UINT32_MAX)
    return -1;
  *value = (uint32_t)parsed;
  return 0;
}

static int
parse_uint(const char *text, unsigned int *value)
{
  char *end = NULL;
  unsigned long parsed;

  if (!text || !*text || text[0] == '-' || text[0] == '+')
    return -1;
  errno = 0;
  parsed = strtoul(text, &end, 10);
  if (errno || !end || end == text || *end || parsed > UINT_MAX)
    return -1;
  *value = (unsigned int)parsed;
  return 0;
}

static int
parse_size(const char *text, size_t *value)
{
  char *end = NULL;
  unsigned long long parsed;

  if (!text || !*text || text[0] == '-' || text[0] == '+')
    return -1;
  errno = 0;
  parsed = strtoull(text, &end, 10);
  if (errno || !end || end == text || *end || parsed > SIZE_MAX)
    return -1;
  *value = (size_t)parsed;
  return 0;
}

static int
parse_u64(const char *text, uint64_t *value)
{
  char *end = NULL;
  unsigned long long parsed;

  if (!text || !*text || text[0] == '-' || text[0] == '+')
    return -1;
  errno = 0;
  parsed = strtoull(text, &end, 10);
  if (errno || !end || end == text || *end || parsed > UINT64_MAX)
    return -1;
  *value = (uint64_t)parsed;
  return 0;
}

static int
parse_ulong_hex(const char *text, unsigned long *value)
{
  char *end = NULL;
  unsigned long parsed;

  if (!text || strncmp(text, "0x", 2) != 0)
    return -1;
  errno = 0;
  parsed = strtoul(text + 2, &end, 16);
  if (errno || !end || end == text + 2 || *end)
    return -1;
  *value = parsed;
  return 0;
}

static int
parse_double(const char *text, double *value)
{
  char *end = NULL;
  double parsed;

  if (!text || !*text)
    return -1;
  errno = 0;
  parsed = strtod(text, &end);
  if (errno || !end || end == text || *end || !isfinite(parsed))
    return -1;
  *value = parsed;
  return 0;
}

static size_t
split_fields(char *line, char **fields, size_t capacity)
{
  size_t count = 0;
  char *p = line;

  if (!line || !*line || capacity == 0)
    return 0;

  for (;;)
  {
    char *tab;

    if (count == capacity)
      return capacity + 1u;
    fields[count++] = p;
    tab = strchr(p, '\t');
    if (!tab)
      break;
    *tab = '\0';
    p = tab + 1;
  }
  return count;
}

static RawosdFeedbackEvent
feedback_event(const char *name)
{
  static const char *const names[RAWOSD_FEEDBACK_EVENT_LAST] = {
    "monitor",
    "master-count",
    "master-factor",
    "move",
    "resize",
    "view",
    "layout",
    "floating",
    "fullscreen",
    "tag",
    "toggle-tag",
    "tag-monitor",
  };
  size_t i;

  for (i = 0; i < RAWOSD_FEEDBACK_EVENT_LAST; ++i)
    if (strcmp(name, names[i]) == 0)
      return (RawosdFeedbackEvent)i;
  return RAWOSD_FEEDBACK_EVENT_LAST;
}

const char *
rawosd_feedback_event_name(RawosdFeedbackEvent event)
{
  static const char *const names[RAWOSD_FEEDBACK_EVENT_LAST] = {
    "monitor",
    "master-count",
    "master-factor",
    "move",
    "resize",
    "view",
    "layout",
    "floating",
    "fullscreen",
    "tag",
    "toggle-tag",
    "tag-monitor",
  };

  if (event < 0 || event >= RAWOSD_FEEDBACK_EVENT_LAST)
    return NULL;
  return names[event];
}

static int
feedback_semantics_valid(const RawosdFeedback *feedback)
{
  if (!feedback || feedback->serial == 0 || feedback->monitor < 0)
    return 0;

  switch (feedback->event)
  {
    case RAWOSD_FEEDBACK_MONITOR:
      return feedback->arg0 == feedback->monitor && feedback->arg1 == 0;
    case RAWOSD_FEEDBACK_MASTER_COUNT:
      return feedback->arg0 >= 0 && feedback->arg1 == 0;
    case RAWOSD_FEEDBACK_MASTER_FACTOR:
      return feedback->arg0 >= 100000 && feedback->arg0 <= 900000 &&
             feedback->arg1 == 0;
    case RAWOSD_FEEDBACK_MOVE:
      return 1;
    case RAWOSD_FEEDBACK_RESIZE:
      return feedback->arg0 > 0 && feedback->arg1 > 0;
    case RAWOSD_FEEDBACK_VIEW:
    case RAWOSD_FEEDBACK_TAG:
    case RAWOSD_FEEDBACK_TOGGLE_TAG:
      return feedback->arg0 > 0 && feedback->arg1 == 0;
    case RAWOSD_FEEDBACK_LAYOUT:
      return feedback->arg0 >= 0 && feedback->arg0 < 6 && feedback->arg1 == 0;
    case RAWOSD_FEEDBACK_FLOATING:
    case RAWOSD_FEEDBACK_FULLSCREEN:
      return (feedback->arg0 == 0 || feedback->arg0 == 1) &&
             feedback->arg1 == 0;
    case RAWOSD_FEEDBACK_TAG_MONITOR:
      return feedback->arg0 >= 0 && feedback->arg1 == 0;
    case RAWOSD_FEEDBACK_EVENT_LAST:
      return 0;
  }
  return 0;
}

int
rawosd_feedback_parse(const char *record, RawosdFeedback *feedback, FILE *err)
{
  char *copy;
  char *fields[6];
  size_t count;
  RawosdFeedback parsed;

  if (!record || !feedback)
    return fail(err, "invalid feedback parser arguments");
  copy = strdup(record);
  if (!copy)
    return fail(err, "out of memory parsing feedback");
  if (strchr(copy, '\n'))
  {
    char *newline = strchr(copy, '\n');
    if (newline[1] != '\0')
    {
      free(copy);
      return fail(err, "feedback record contains multiple lines");
    }
    *newline = '\0';
  }

  count = split_fields(copy, fields, 6);
  if (count != 6 || strcmp(fields[0], "rawm-feedback-v1") != 0 ||
      parse_u64(fields[1], &parsed.serial) < 0 ||
      (parsed.event = feedback_event(fields[2])) == RAWOSD_FEEDBACK_EVENT_LAST ||
      parse_int(fields[3], &parsed.monitor) < 0 ||
      parse_i32(fields[4], &parsed.arg0) < 0 ||
      parse_i32(fields[5], &parsed.arg1) < 0 ||
      !feedback_semantics_valid(&parsed))
  {
    free(copy);
    return fail(err, "malformed or unsupported feedback record");
  }
  free(copy);
  *feedback = parsed;
  return 0;
}

static void
monitor_destroy(RawosdMonitorState *monitor, size_t tag_count)
{
  size_t i;

  if (!monitor)
    return;
  for (i = 0; i < tag_count; ++i)
    free(monitor->tags ? monitor->tags[i] : NULL);
  free(monitor->tags);
  monitor->tags = NULL;
}

void
rawosd_state_destroy(RawosdState *state)
{
  size_t i;

  if (!state)
    return;
  for (i = 0; i < state->monitor_count; ++i)
    monitor_destroy(&state->monitors[i], state->tag_count);
  free(state->monitors);
  memset(state, 0, sizeof(*state));
  state->selected_monitor = -1;
}

static RawosdMonitorState *
state_add_monitor(RawosdState *state)
{
  RawosdMonitorState *monitors;
  RawosdMonitorState *monitor;

  monitors = realloc(state->monitors,
                     (state->monitor_count + 1u) * sizeof(*monitors));
  if (!monitors)
    return NULL;
  state->monitors = monitors;
  monitor = &state->monitors[state->monitor_count++];
  memset(monitor, 0, sizeof(*monitor));
  monitor->visible_count = UINT_MAX;
  monitor->tags = calloc(state->tag_count, sizeof(*monitor->tags));
  if (!monitor->tags)
  {
    --state->monitor_count;
    return NULL;
  }
  return monitor;
}

static RawosdMonitorState *
state_monitor_mutable(RawosdState *state, int number)
{
  size_t i;

  for (i = 0; i < state->monitor_count; ++i)
    if (state->monitors[i].number == number)
      return &state->monitors[i];
  return NULL;
}

const RawosdMonitorState *
rawosd_state_monitor(const RawosdState *state, int number)
{
  size_t i;

  if (!state)
    return NULL;
  for (i = 0; i < state->monitor_count; ++i)
    if (state->monitors[i].number == number)
      return &state->monitors[i];
  return NULL;
}

static char *
decode_text(const char *text)
{
  size_t length;
  size_t i;
  size_t out = 0;
  char *decoded;

  length = strlen(text);
  decoded = malloc(length + 1u);
  if (!decoded)
    return NULL;

  for (i = 0; i < length; ++i)
  {
    if (text[i] == '%' && i + 2u < length)
    {
      char hex[3] = { text[i + 1u], text[i + 2u], '\0' };
      char *end = NULL;
      long value;

      errno = 0;
      value = strtol(hex, &end, 16);
      if (!errno && end && *end == '\0' && value != 0)
      {
        decoded[out++] = (char)value;
        i += 2u;
        continue;
      }
    }
    decoded[out++] = text[i];
  }
  decoded[out] = '\0';
  return decoded;
}

static int
layout_valid(const char *name)
{
  static const char *const names[] = {
    "tile", "floating", "monocle", "bstack", "bstack_horizontal", "grid",
  };
  size_t i;

  for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    if (strcmp(name, names[i]) == 0)
      return 1;
  return 0;
}

static int
parse_monitor_record(RawosdState *state, char **fields, size_t count)
{
  RawosdMonitorState *monitor;
  int selected;

  if (count != 18)
    return -1;
  monitor = state_add_monitor(state);
  if (!monitor)
    return -1;
  if (parse_int(fields[1], &monitor->number) < 0 ||
      parse_int(fields[2], &selected) < 0 ||
      (selected != 0 && selected != 1) ||
      parse_int(fields[3], &monitor->x) < 0 ||
      parse_int(fields[4], &monitor->y) < 0 ||
      parse_int(fields[5], &monitor->width) < 0 ||
      parse_int(fields[6], &monitor->height) < 0 ||
      monitor->width <= 0 || monitor->height <= 0 ||
      parse_int(fields[7], &monitor->work_x) < 0 ||
      parse_int(fields[8], &monitor->work_y) < 0 ||
      parse_int(fields[9], &monitor->work_width) < 0 ||
      parse_int(fields[10], &monitor->work_height) < 0 ||
      monitor->work_width <= 0 || monitor->work_height <= 0 ||
      parse_u32_hex(fields[11], &monitor->viewed) < 0 ||
      parse_u32_hex(fields[12], &monitor->occupied) < 0 ||
      parse_u32_hex(fields[13], &monitor->urgent) < 0 ||
      strlen(fields[14]) >= sizeof(monitor->layout) || !layout_valid(fields[14]) ||
      parse_ulong_hex(fields[15], &monitor->focused) < 0 ||
      parse_int(fields[16], &monitor->nmaster) < 0 || monitor->nmaster < 0 ||
      parse_double(fields[17], &monitor->mfact) < 0 ||
      monitor->mfact < 0.1 || monitor->mfact > 0.9)
    return -1;
  monitor->selected = selected;
  memcpy(monitor->layout, fields[14], strlen(fields[14]) + 1u);
  return 0;
}

static int
parse_stack_record(RawosdState *state, char **fields, size_t count)
{
  RawosdMonitorState *monitor;
  int number;

  if (count != 4 || parse_int(fields[1], &number) < 0)
    return -1;
  monitor = state_monitor_mutable(state, number);
  if (!monitor || monitor->visible_count != UINT_MAX ||
      parse_uint(fields[2], &monitor->focused_index) < 0 ||
      parse_uint(fields[3], &monitor->visible_count) < 0 ||
      monitor->focused_index > monitor->visible_count)
    return -1;
  return 0;
}

static int
parse_tag_record(RawosdState *state, char **fields, size_t count)
{
  RawosdMonitorState *monitor;
  int number;
  size_t index;
  char *decoded;

  if (count != 4 || parse_int(fields[1], &number) < 0 ||
      parse_size(fields[2], &index) < 0 || index == 0 ||
      index > state->tag_count)
    return -1;
  monitor = state_monitor_mutable(state, number);
  if (!monitor || monitor->tags[index - 1u])
    return -1;
  decoded = decode_text(fields[3]);
  if (!decoded)
    return -1;
  monitor->tags[index - 1u] = decoded;
  return 0;
}

int
rawosd_state_parse(const char *snapshot, RawosdState *state, FILE *err)
{
  RawosdState parsed = { .selected_monitor = -1 };
  char *copy = NULL;
  char *save = NULL;
  char *line;
  size_t line_number = 0;
  size_t i;

  if (!snapshot || !state)
    return fail(err, "invalid state parser arguments");
  if (!*snapshot || strstr(snapshot, "\n\n") != NULL)
    return fail(err, "malformed or unsupported state snapshot");
  copy = strdup(snapshot);
  if (!copy)
    return fail(err, "out of memory parsing state");

  line = strtok_r(copy, "\n", &save);
  if (!line || strcmp(line, "rawm-state-v2") != 0)
    goto malformed;
  line_number++;

  line = strtok_r(NULL, "\n", &save);
  if (!line)
    goto malformed;
  {
    char *fields[2];
    if (split_fields(line, fields, 2) != 2 ||
        strcmp(fields[0], "tag-count") != 0 ||
        parse_size(fields[1], &parsed.tag_count) < 0 ||
        parsed.tag_count == 0 || parsed.tag_count > 31)
      goto malformed;
  }
  line_number++;

  line = strtok_r(NULL, "\n", &save);
  if (!line)
    goto malformed;
  {
    char *fields[2];
    if (split_fields(line, fields, 2) != 2 ||
        strcmp(fields[0], "selected-monitor") != 0 ||
        parse_int(fields[1], &parsed.selected_monitor) < 0 ||
        parsed.selected_monitor < -1)
      goto malformed;
  }
  line_number++;

  while ((line = strtok_r(NULL, "\n", &save)) != NULL)
  {
    char *fields[18];
    size_t count;

    line_number++;
    count = split_fields(line, fields, 18);
    if (count == 0 || count > 18)
      goto malformed;
    if (strcmp(fields[0], "monitor") == 0)
    {
      if (parse_monitor_record(&parsed, fields, count) < 0)
        goto malformed;
    }
    else if (strcmp(fields[0], "stack") == 0)
    {
      if (parse_stack_record(&parsed, fields, count) < 0)
        goto malformed;
    }
    else if (strcmp(fields[0], "tag") == 0)
    {
      if (parse_tag_record(&parsed, fields, count) < 0)
        goto malformed;
    }
    else
      goto malformed;
  }

  {
    uint32_t valid_mask = parsed.tag_count == 31
                            ? 0x7fffffffu
                            : ((1u << parsed.tag_count) - 1u);
    size_t selected_count = 0;

    for (i = 0; i < parsed.monitor_count; ++i)
    {
      size_t j;
      RawosdMonitorState *monitor = &parsed.monitors[i];
      if (monitor->visible_count == UINT_MAX ||
          (monitor->viewed & ~valid_mask) != 0 ||
          (monitor->occupied & ~valid_mask) != 0 ||
          (monitor->urgent & ~valid_mask) != 0)
        goto malformed;
      if (monitor->selected)
      {
        ++selected_count;
        if (monitor->number != parsed.selected_monitor)
          goto malformed;
      }
      for (j = 0; j < parsed.tag_count; ++j)
        if (!monitor->tags[j])
          goto malformed;
    }
    if ((parsed.selected_monitor < 0 && selected_count != 0) ||
        (parsed.selected_monitor >= 0 &&
         (selected_count != 1 ||
          !rawosd_state_monitor(&parsed, parsed.selected_monitor))))
      goto malformed;
  }

  free(copy);
  rawosd_state_destroy(state);
  *state = parsed;
  return 0;

malformed:
  free(copy);
  rawosd_state_destroy(&parsed);
  if (err)
    fprintf(err, "rawosd: malformed or unsupported state snapshot near line %zu\n",
            line_number + 1u);
  return -1;
}
