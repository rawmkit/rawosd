#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void
sleep_ms(unsigned int milliseconds)
{
  struct timespec delay;

  delay.tv_sec = milliseconds / 1000u;
  delay.tv_nsec = (long)(milliseconds % 1000u) * 1000000l;
  while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
    ;
}

static Window
find_rawosd(Display *display)
{
  Window root = RootWindow(display, DefaultScreen(display));
  Window root_return;
  Window parent_return;
  Window *children = NULL;
  unsigned int count = 0;
  unsigned int i;
  Window result = None;

  if (!XQueryTree(display, root, &root_return, &parent_return,
                  &children, &count))
    return None;
  for (i = 0; i < count; ++i)
  {
    XClassHint hint;
    memset(&hint, 0, sizeof(hint));
    if (XGetClassHint(display, children[i], &hint))
    {
      if ((hint.res_name && strcmp(hint.res_name, "rawosd") == 0) ||
          (hint.res_class && strcmp(hint.res_class, "rawosd") == 0))
        result = children[i];
      if (hint.res_name)
        XFree(hint.res_name);
      if (hint.res_class)
        XFree(hint.res_class);
      if (result != None)
        break;
    }
  }
  if (children)
    XFree(children);
  return result;
}

int
main(int argc, char **argv)
{
  Display *display;
  int want_viewable;
  int timeout_ms;
  int elapsed = 0;

  if (argc != 3 ||
      (strcmp(argv[1], "viewable") != 0 && strcmp(argv[1], "hidden") != 0))
  {
    fprintf(stderr, "usage: %s viewable|hidden TIMEOUT-MS\n", argv[0]);
    return EXIT_FAILURE;
  }
  want_viewable = strcmp(argv[1], "viewable") == 0;
  timeout_ms = atoi(argv[2]);
  if (timeout_ms < 0)
    return EXIT_FAILURE;
  display = XOpenDisplay(NULL);
  if (!display)
    return EXIT_FAILURE;

  while (elapsed <= timeout_ms)
  {
    Window window = find_rawosd(display);
    if (window != None)
    {
      XWindowAttributes attributes;
      if (XGetWindowAttributes(display, window, &attributes))
      {
        int viewable = attributes.map_state == IsViewable;
        if (viewable == want_viewable)
        {
          XCloseDisplay(display);
          return EXIT_SUCCESS;
        }
      }
    }
    sleep_ms(20);
    elapsed += 20;
  }
  XCloseDisplay(display);
  return EXIT_FAILURE;
}
