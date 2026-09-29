#ifndef RAWOSD_CONTROL_H
#define RAWOSD_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <X11/Xlib.h>

#include "rawosd-protocol.h"

typedef struct {
  Display *display;
  int screen;
  Window owner;
  Atom selection_atom;
  Atom version_atom;
  Atom state_atom;
  Atom feedback_atom;
  Atom utf8_atom;
  uint64_t last_serial;
  bool have_serial;
} RawosdControl;

int rawosd_control_init(RawosdControl *control, Display *display, int screen,
                        FILE *err);
void rawosd_control_detach(RawosdControl *control);
int rawosd_control_refresh_owner(RawosdControl *control, FILE *err);
int rawosd_control_read_state(RawosdControl *control, RawosdState *state,
                              FILE *err);
int rawosd_control_read_feedback(RawosdControl *control,
                                 RawosdFeedback *feedback, FILE *err);
bool rawosd_control_is_feedback_event(const RawosdControl *control,
                                      const XPropertyEvent *event);
bool rawosd_control_is_owner_destroy(const RawosdControl *control,
                                     const XDestroyWindowEvent *event);

#endif
