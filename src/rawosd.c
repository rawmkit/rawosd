#include "rawosd-config.h"
#include "rawosd-control.h"
#include "rawosd-present.h"
#include "rawosd-protocol.h"
#include "rawosd-render.h"

#include <X11/Xlib.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef VERSION
#define VERSION "unknown"
#endif

#define OWNER_POLL_MS 500u

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_requested = 0;

static uint64_t
monotonic_ms(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    return 0;
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void
signal_handler(int signal_number)
{
  if (signal_number == SIGHUP)
    reload_requested = 1;
  else
    running = 0;
}

static int
install_signals(void)
{
  struct sigaction action;

  memset(&action, 0, sizeof(action));
  action.sa_handler = signal_handler;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGHUP, &action, NULL) < 0 ||
      sigaction(SIGINT, &action, NULL) < 0 ||
      sigaction(SIGTERM, &action, NULL) < 0)
    return -1;
  return 0;
}

static int
xerror(Display *display, XErrorEvent *event)
{
  char message[256];

  if (event->error_code == BadWindow)
    return 0;
  XGetErrorText(display, event->error_code, message, sizeof(message));
  fprintf(stderr, "rawosd: X error: %s (request %u.%u)\n",
          message, event->request_code, event->minor_code);
  return 0;
}

static void
usage(FILE *stream)
{
  fprintf(stream, "usage: rawosd [-C] [-c path] [-v]\n");
}

static int
minimum_timeout(int first, int second)
{
  if (first < 0)
    return second;
  if (second < 0)
    return first;
  return first < second ? first : second;
}

static int
reload_config(RawosdConfig *config, RawosdRenderer *renderer,
              const char *explicit_path)
{
  RawosdConfig replacement;

  rawosd_config_init(&replacement);
  if (rawosd_config_load(&replacement, explicit_path, stderr) < 0)
  {
    rawosd_config_destroy(&replacement);
    fputs("rawosd: keeping previous configuration\n", stderr);
    return -1;
  }
  if (rawosd_renderer_reconfigure(renderer, &replacement, stderr) < 0)
  {
    rawosd_config_destroy(&replacement);
    fputs("rawosd: keeping previous renderer configuration\n", stderr);
    return -1;
  }
  rawosd_config_destroy(config);
  *config = replacement;
  renderer->config = config;
  return 0;
}

static void
handle_feedback(RawosdControl *control, RawosdRenderer *renderer,
                RawosdConfig *config)
{
  RawosdFeedback feedback;
  RawosdState state = { .selected_monitor = -1 };
  RawosdPresentation presentation;
  int rc;

  rc = rawosd_control_read_feedback(control, &feedback, stderr);
  if (rc <= 0)
    return;
  if (rawosd_control_read_state(control, &state, stderr) < 0)
    return;
  if (rawosd_present_format(&feedback, &state, config, &presentation,
                            stderr) == 0 && presentation.show)
    (void)rawosd_renderer_show(renderer, &presentation, &state, stderr);
  rawosd_state_destroy(&state);
}

int
main(int argc, char **argv)
{
  const char *config_path = NULL;
  RawosdConfig config;
  RawosdControl control = {0};
  RawosdRenderer renderer = {0};
  Display *display;
  int screen;
  int option;
  bool check = false;
  uint64_t next_owner_poll;
  struct pollfd pfd;
  int exit_status = EXIT_SUCCESS;

  while ((option = getopt(argc, argv, "Cc:hv")) != -1)
  {
    switch (option)
    {
      case 'C':
        check = true;
        break;
      case 'c':
        config_path = optarg;
        break;
      case 'h':
        usage(stdout);
        return EXIT_SUCCESS;
      case 'v':
        printf("rawosd-%s\n", VERSION);
        return EXIT_SUCCESS;
      default:
        usage(stderr);
        return EXIT_FAILURE;
    }
  }
  if (optind != argc)
  {
    usage(stderr);
    return EXIT_FAILURE;
  }

  rawosd_config_init(&config);
  if (rawosd_config_load(&config, config_path, stderr) < 0)
  {
    rawosd_config_destroy(&config);
    return EXIT_FAILURE;
  }
  if (check)
  {
    rawosd_config_destroy(&config);
    return EXIT_SUCCESS;
  }
  display = XOpenDisplay(NULL);
  if (!display)
  {
    fputs("rawosd: cannot open display\n", stderr);
    rawosd_config_destroy(&config);
    return EXIT_FAILURE;
  }
  XSetErrorHandler(xerror);
  screen = DefaultScreen(display);
  if (rawosd_control_init(&control, display, screen, stderr) < 0 ||
      rawosd_renderer_init(&renderer, display, screen, &config, stderr) < 0 ||
      install_signals() < 0)
  {
    rawosd_renderer_destroy(&renderer);
    rawosd_config_destroy(&config);
    XCloseDisplay(display);
    return EXIT_FAILURE;
  }

  next_owner_poll = 0;
  pfd.fd = ConnectionNumber(display);
  pfd.events = POLLIN;
  pfd.revents = 0;

  while (running)
  {
    uint64_t now = monotonic_ms();
    int owner_timeout;
    int render_timeout;
    int timeout;
    int rc;

    if (reload_requested)
    {
      reload_requested = 0;
      (void)reload_config(&config, &renderer, config_path);
    }

    if (now >= next_owner_poll)
    {
      Window previous = control.owner;
      rc = rawosd_control_refresh_owner(&control, stderr);
      if (control.owner != previous)
        rawosd_renderer_hide(&renderer);
      (void)rc;
      next_owner_poll = now + OWNER_POLL_MS;
    }

    while (XPending(display))
    {
      XEvent event;
      XNextEvent(display, &event);
      if (event.type == PropertyNotify &&
          rawosd_control_is_feedback_event(&control, &event.xproperty))
        handle_feedback(&control, &renderer, &config);
      else if (event.type == DestroyNotify &&
               rawosd_control_is_owner_destroy(&control, &event.xdestroywindow))
      {
        rawosd_control_detach(&control);
        rawosd_renderer_hide(&renderer);
        next_owner_poll = 0;
      }
      else if (event.type == Expose &&
               rawosd_renderer_is_expose(&renderer, &event.xexpose))
        rawosd_renderer_redraw(&renderer);
    }

    rawosd_renderer_expire(&renderer);
    now = monotonic_ms();
    owner_timeout = now >= next_owner_poll
                      ? 0
                      : (int)(next_owner_poll - now);
    render_timeout = rawosd_renderer_timeout(&renderer);
    timeout = minimum_timeout(owner_timeout, render_timeout);
    rc = poll(&pfd, 1, timeout);
    if (rc < 0 && errno != EINTR)
    {
      fprintf(stderr, "rawosd: poll failed: %s\n", strerror(errno));
      exit_status = EXIT_FAILURE;
      break;
    }
  }

  rawosd_renderer_destroy(&renderer);
  rawosd_config_destroy(&config);
  XCloseDisplay(display);
  return exit_status;
}
