#include "rawosd-control.h"
#include "rawosd-protocol.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
expect(int condition, const char *message)
{
  if (!condition)
  {
    fprintf(stderr, "control owner test failed: %s\n", message);
    exit(EXIT_FAILURE);
  }
}

static Window
make_owner(Display *display, Atom selection, Atom version_atom,
           Atom state_atom, Atom feedback_atom, Atom utf8_atom,
           unsigned long version, const char *feedback)
{
  int screen = DefaultScreen(display);
  Window root = RootWindow(display, screen);
  XSetWindowAttributes attributes;
  Window owner;
  static const char state[] =
    "rawm-state-v1\n"
    "tag-count\t1\n"
    "selected-monitor\t0\n"
    "monitor\t0\t1\t0\t0\t800\t600\t00000001\t00000001\t00000000\ttile\t0x0\t1\t0.550000\n"
    "stack\t0\t0\t0\n"
    "tag\t0\t1\t1\n";

  memset(&attributes, 0, sizeof(attributes));
  owner = XCreateWindow(display, root, -1, -1, 1, 1, 0, 0, InputOnly,
                        CopyFromParent, 0, &attributes);
  expect(owner != None, "could not create fake owner");
  XChangeProperty(display, owner, version_atom, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&version, 1);
  XChangeProperty(display, owner, state_atom, utf8_atom, 8, PropModeReplace,
                  (const unsigned char *)state, (int)strlen(state));
  XChangeProperty(display, owner, feedback_atom, utf8_atom, 8, PropModeReplace,
                  (const unsigned char *)feedback, (int)strlen(feedback));
  XSetSelectionOwner(display, selection, owner, CurrentTime);
  XSync(display, False);
  expect(XGetSelectionOwner(display, selection) == owner,
         "fake owner did not acquire selection");
  return owner;
}

int
main(void)
{
  Display *display = XOpenDisplay(NULL);
  RawosdControl control;
  RawosdFeedback feedback;
  RawosdState state = { .selected_monitor = -1 };
  Atom selection;
  Atom version_atom;
  Atom state_atom;
  Atom feedback_atom;
  Atom utf8_atom;
  Window owner;
  XEvent event;
  int saw_feedback = 0;
  int saw_destroy = 0;
  unsigned long version;

  expect(display != NULL, "cannot open display");
  selection = XInternAtom(display, "_RAWM_CONTROL_S0", False);
  version_atom = XInternAtom(display, "_RAWM_CONTROL_VERSION", False);
  state_atom = XInternAtom(display, "_RAWM_STATE", False);
  feedback_atom = XInternAtom(display, "_RAWM_FEEDBACK", False);
  utf8_atom = XInternAtom(display, "UTF8_STRING", False);

  owner = make_owner(display, selection, version_atom, state_atom,
                     feedback_atom, utf8_atom, 1,
                     "rawm-feedback-v1\t1\tview\t0\t1\t0\n");
  expect(rawosd_control_init(&control, display, 0, stderr) == 0,
         "control init failed");
  expect(rawosd_control_refresh_owner(&control, stderr) == 1 &&
         control.owner == owner,
         "owner attach failed");
  expect(rawosd_control_read_feedback(&control, &feedback, stderr) == 0,
         "startup feedback was replayed");
  expect(rawosd_control_read_state(&control, &state, stderr) == 0 &&
         state.monitor_count == 1,
         "state read failed");
  rawosd_state_destroy(&state);

  {
    static const char next[] =
      "rawm-feedback-v1\t2\tlayout\t0\t5\t0\n";
    XChangeProperty(display, owner, feedback_atom, utf8_atom, 8,
                    PropModeReplace, (const unsigned char *)next,
                    (int)strlen(next));
    XSync(display, False);
  }
  while (XPending(display))
  {
    XNextEvent(display, &event);
    if (event.type == PropertyNotify &&
        rawosd_control_is_feedback_event(&control, &event.xproperty))
      saw_feedback = 1;
  }
  expect(saw_feedback, "feedback PropertyNotify not observed");
  expect(rawosd_control_read_feedback(&control, &feedback, stderr) == 1 &&
         feedback.serial == 2 && feedback.event == RAWOSD_FEEDBACK_LAYOUT &&
         feedback.arg0 == 5,
         "new feedback read differs");
  expect(rawosd_control_read_feedback(&control, &feedback, stderr) == 0,
         "duplicate serial was replayed");

  XDestroyWindow(display, owner);
  XSync(display, False);
  while (XPending(display))
  {
    XNextEvent(display, &event);
    if (event.type == DestroyNotify &&
        rawosd_control_is_owner_destroy(&control, &event.xdestroywindow))
      saw_destroy = 1;
  }
  expect(saw_destroy, "owner DestroyNotify not observed");
  rawosd_control_detach(&control);
  expect(control.owner == None && !control.have_serial,
         "detach did not reset owner state");

  owner = make_owner(display, selection, version_atom, state_atom,
                     feedback_atom, utf8_atom, 2,
                     "rawm-feedback-v1\t1\tview\t0\t1\t0\n");
  expect(rawosd_control_refresh_owner(&control, NULL) < 0 &&
         control.owner == None,
         "unsupported control version accepted");
  version = 1;
  XChangeProperty(display, owner, version_atom, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&version, 1);
  XSync(display, False);
  expect(rawosd_control_refresh_owner(&control, stderr) == 1 &&
         control.owner == owner,
         "compatible replacement owner rejected");

  XDestroyWindow(display, owner);
  XCloseDisplay(display);
  puts("control owner tests: ok");
  return EXIT_SUCCESS;
}
