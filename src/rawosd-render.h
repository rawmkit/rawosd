#ifndef RAWOSD_RENDER_H
#define RAWOSD_RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>

#include "rawosd-config.h"
#include "rawosd-present.h"
#include "rawosd-protocol.h"

#define RAWOSD_FONT_FALLBACK_MAX 32u
#define RAWOSD_FONT_MISSING_MAX 128u

typedef struct {
  Display *display;
  int screen;
  Window root;
  Window window;
  Pixmap pixmap;
  GC gc;
  unsigned int width;
  unsigned int height;
  bool visible;
  uint64_t deadline_ms;
  RawosdConfig *config;
  XftColor colors[RAWOSD_SCHEME_LAST][RAWOSD_COLOR_LAST];
  bool color_allocated[RAWOSD_SCHEME_LAST][RAWOSD_COLOR_LAST];
  struct {
    int height;
    XftFont *primary;
    FcPattern *pattern;
    XftFont *fallbacks[RAWOSD_FONT_FALLBACK_MAX];
    size_t fallback_count;
    FcChar32 missing[RAWOSD_FONT_MISSING_MAX];
  } font;
} RawosdRenderer;

int rawosd_renderer_init(RawosdRenderer *renderer, Display *display, int screen,
                         RawosdConfig *config, FILE *err);
void rawosd_renderer_destroy(RawosdRenderer *renderer);
int rawosd_renderer_reconfigure(RawosdRenderer *renderer,
                                RawosdConfig *config, FILE *err);
int rawosd_renderer_show(RawosdRenderer *renderer,
                         const RawosdPresentation *presentation,
                         const RawosdState *state, FILE *err);
void rawosd_renderer_hide(RawosdRenderer *renderer);
void rawosd_renderer_expire(RawosdRenderer *renderer);
int rawosd_renderer_timeout(const RawosdRenderer *renderer);
bool rawosd_renderer_is_expose(const RawosdRenderer *renderer,
                               const XExposeEvent *event);
void rawosd_renderer_redraw(RawosdRenderer *renderer);

#endif
