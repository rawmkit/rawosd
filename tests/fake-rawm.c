#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void
fail(const char *message)
{
  fprintf(stderr, "fake-rawm: %s\n", message);
  exit(EXIT_FAILURE);
}

static void
sleep_ms(unsigned int milliseconds)
{
  struct timespec delay;

  delay.tv_sec = milliseconds / 1000u;
  delay.tv_nsec = (long)(milliseconds % 1000u) * 1000000l;
  while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
    ;
}

static void
write_marker(const char *path)
{
  FILE *stream = fopen(path, "w");

  if (!stream)
    fail("cannot write ready marker");
  fputs("ready\n", stream);
  if (fclose(stream) != 0)
    fail("cannot finalize ready marker");
}

int
main(int argc, char **argv)
{
  Display *display;
  int screen;
  Window root;
  Window owner;
  XSetWindowAttributes attributes;
  Atom selection;
  Atom version_atom;
  Atom state_atom;
  Atom feedback_atom;
  Atom utf8_atom;
  unsigned long version = 1;
  unsigned int waits = 0;
  static const char state[] =
    "rawm-state-v2\n"
    "tag-count\t2\n"
    "selected-monitor\t0\n"
    "monitor\t0\t1\t0\t0\t800\t600\t0\t0\t800\t600\t00000001\t00000001\t00000000\ttile\t0x0\t1\t0.550000\n"
    "stack\t0\t0\t0\n"
    "tag\t0\t1\twork\n"
    "tag\t0\t2\tweb\n";
  static const char baseline[] =
    "rawm-feedback-v1\t1\tview\t0\t1\t0\n";
  static const char event[] =
    "rawm-feedback-v1\t2\tlayout\t0\t5\t0\n";

  if (argc != 3)
  {
    fprintf(stderr, "usage: %s READY-FILE TRIGGER-FILE\n", argv[0]);
    return EXIT_FAILURE;
  }
  display = XOpenDisplay(NULL);
  if (!display)
    fail("cannot open display");
  screen = DefaultScreen(display);
  root = RootWindow(display, screen);
  selection = XInternAtom(display, "_RAWM_CONTROL_S0", False);
  version_atom = XInternAtom(display, "_RAWM_CONTROL_VERSION", False);
  state_atom = XInternAtom(display, "_RAWM_STATE", False);
  feedback_atom = XInternAtom(display, "_RAWM_FEEDBACK", False);
  utf8_atom = XInternAtom(display, "UTF8_STRING", False);

  memset(&attributes, 0, sizeof(attributes));
  owner = XCreateWindow(display, root, -1, -1, 1, 1, 0, 0, InputOnly,
                        CopyFromParent, 0, &attributes);
  if (owner == None)
    fail("cannot create control owner");
  XChangeProperty(display, owner, version_atom, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&version, 1);
  XChangeProperty(display, owner, state_atom, utf8_atom, 8, PropModeReplace,
                  (const unsigned char *)state, (int)strlen(state));
  XChangeProperty(display, owner, feedback_atom, utf8_atom, 8, PropModeReplace,
                  (const unsigned char *)baseline, (int)strlen(baseline));
  XSetSelectionOwner(display, selection, owner, CurrentTime);
  XSync(display, False);
  if (XGetSelectionOwner(display, selection) != owner)
    fail("cannot acquire control selection");
  write_marker(argv[1]);

  while (access(argv[2], F_OK) != 0)
  {
    if (++waits > 250)
      fail("trigger timed out");
    sleep_ms(20);
  }
  XChangeProperty(display, owner, feedback_atom, utf8_atom, 8, PropModeReplace,
                  (const unsigned char *)event, (int)strlen(event));
  XFlush(display);
  sleep_ms(500);
  XDestroyWindow(display, owner);
  XSync(display, False);
  XCloseDisplay(display);
  return EXIT_SUCCESS;
}
