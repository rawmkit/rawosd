#include "rawosd-control.h"

#include <X11/Xatom.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define RAWM_CONTROL_VERSION 1ul

static unsigned char *
get_property(Display *display, Window window, Atom property, Atom type,
             int *format, unsigned long *count, FILE *err)
{
  Atom actual = None;
  unsigned long after = 0;
  unsigned char *data = NULL;
  int status;

  status = XGetWindowProperty(display, window, property, 0, 65536,
                              False, type, &actual, format, count, &after,
                              &data);
  if (status != Success || actual != type || after != 0)
  {
    if (data)
      XFree(data);
    if (err)
      fputs("rawosd: could not read rawm control property\n", err);
    return NULL;
  }
  return data;
}

static int
read_version(RawosdControl *control, FILE *err)
{
  unsigned char *data;
  unsigned long count = 0;
  int format = 0;
  unsigned long version;

  data = get_property(control->display, control->owner, control->version_atom,
                      XA_CARDINAL, &format, &count, err);
  if (!data || format != 32 || count != 1)
  {
    if (data)
      XFree(data);
    if (err)
      fputs("rawosd: rawm control version is unavailable\n", err);
    return -1;
  }
  version = *(unsigned long *)data;
  XFree(data);
  if (version != RAWM_CONTROL_VERSION)
  {
    if (err)
      fprintf(err, "rawosd: unsupported rawm control version %lu\n", version);
    return -1;
  }
  return 0;
}

static char *
read_utf8_property(RawosdControl *control, Atom property, FILE *err)
{
  unsigned char *data;
  unsigned long count = 0;
  int format = 0;
  char *copy;

  data = get_property(control->display, control->owner, property,
                      control->utf8_atom, &format, &count, err);
  if (!data || format != 8)
  {
    if (data)
      XFree(data);
    return NULL;
  }
  if (memchr(data, '\0', count) != NULL)
  {
    XFree(data);
    if (err)
      fputs("rawosd: rawm UTF-8 property contains NUL data\n", err);
    return NULL;
  }
  copy = malloc(count + 1u);
  if (!copy)
  {
    XFree(data);
    if (err)
      fputs("rawosd: out of memory reading rawm property\n", err);
    return NULL;
  }
  memcpy(copy, data, count);
  copy[count] = '\0';
  XFree(data);
  return copy;
}

int
rawosd_control_init(RawosdControl *control, Display *display, int screen,
                    FILE *err)
{
  char selection_name[64];
  int n;

  if (!control || !display)
    return -1;
  memset(control, 0, sizeof(*control));
  control->display = display;
  control->screen = screen;
  n = snprintf(selection_name, sizeof(selection_name),
               "_RAWM_CONTROL_S%d", screen);
  if (n < 0 || (size_t)n >= sizeof(selection_name))
  {
    if (err)
      fputs("rawosd: rawm control selection name is too long\n", err);
    return -1;
  }
  control->selection_atom = XInternAtom(display, selection_name, False);
  control->version_atom = XInternAtom(display, "_RAWM_CONTROL_VERSION", False);
  control->state_atom = XInternAtom(display, "_RAWM_STATE", False);
  control->feedback_atom = XInternAtom(display, "_RAWM_FEEDBACK", False);
  control->utf8_atom = XInternAtom(display, "UTF8_STRING", False);
  if (control->selection_atom == None || control->version_atom == None ||
      control->state_atom == None || control->feedback_atom == None ||
      control->utf8_atom == None)
  {
    if (err)
      fputs("rawosd: could not intern rawm control atoms\n", err);
    return -1;
  }
  return 0;
}

void
rawosd_control_detach(RawosdControl *control)
{
  if (!control)
    return;
  control->owner = None;
  control->last_serial = 0;
  control->have_serial = false;
}

int
rawosd_control_refresh_owner(RawosdControl *control, FILE *err)
{
  Window owner;
  char *record;
  RawosdFeedback feedback;

  if (!control || !control->display)
    return -1;
  owner = XGetSelectionOwner(control->display, control->selection_atom);
  if (owner == control->owner)
    return owner == None ? 0 : 1;

  rawosd_control_detach(control);
  if (owner == None)
    return 0;
  control->owner = owner;
  XSelectInput(control->display, owner, PropertyChangeMask | StructureNotifyMask);
  XSync(control->display, False);
  if (XGetSelectionOwner(control->display, control->selection_atom) != owner)
  {
    rawosd_control_detach(control);
    return 0;
  }
  if (read_version(control, err) < 0)
  {
    rawosd_control_detach(control);
    return -1;
  }

  /* Establish a baseline without presenting a stale event at startup. */
  record = read_utf8_property(control, control->feedback_atom, NULL);
  if (record)
  {
    if (rawosd_feedback_parse(record, &feedback, NULL) == 0)
    {
      control->last_serial = feedback.serial;
      control->have_serial = true;
    }
    free(record);
  }
  return 1;
}

int
rawosd_control_read_state(RawosdControl *control, RawosdState *state, FILE *err)
{
  char *snapshot;
  int rc;

  if (!control || control->owner == None)
    return -1;
  snapshot = read_utf8_property(control, control->state_atom, err);
  if (!snapshot)
    return -1;
  rc = rawosd_state_parse(snapshot, state, err);
  free(snapshot);
  return rc;
}

int
rawosd_control_read_feedback(RawosdControl *control,
                             RawosdFeedback *feedback, FILE *err)
{
  char *record;
  RawosdFeedback parsed;
  int rc;

  if (!control || control->owner == None || !feedback)
    return -1;
  record = read_utf8_property(control, control->feedback_atom, err);
  if (!record)
    return -1;
  rc = rawosd_feedback_parse(record, &parsed, err);
  free(record);
  if (rc < 0)
    return -1;

  if (control->have_serial && parsed.serial <= control->last_serial)
    return 0;
  control->last_serial = parsed.serial;
  control->have_serial = true;
  *feedback = parsed;
  return 1;
}

bool
rawosd_control_is_feedback_event(const RawosdControl *control,
                                 const XPropertyEvent *event)
{
  return control && event && control->owner != None &&
         event->window == control->owner &&
         event->atom == control->feedback_atom &&
         event->state == PropertyNewValue;
}

bool
rawosd_control_is_owner_destroy(const RawosdControl *control,
                                const XDestroyWindowEvent *event)
{
  return control && event && control->owner != None &&
         event->window == control->owner;
}
