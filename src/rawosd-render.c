#include "rawosd-render.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

static uint64_t
monotonic_ms(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    return 0;
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static size_t
utf8_decode(const char *text, size_t length, FcChar32 *codepoint)
{
  int consumed;

  if (!text || !length || !codepoint)
    return 0;
  consumed = FcUtf8ToUcs4((const FcChar8 *)text, codepoint,
                          length > INT_MAX ? INT_MAX : (int)length);
  if (consumed <= 0 || (size_t)consumed > length)
  {
    *codepoint = 0xfffdu;
    return 1;
  }
  return (size_t)consumed;
}

static void
missing_indexes(FcChar32 codepoint, size_t *first, size_t *second)
{
  unsigned int hash = (unsigned int)codepoint;

  hash = ((hash >> 16) ^ hash) * 0x21f0aaadu;
  hash = ((hash >> 15) ^ hash) * 0xd35a2d97u;
  hash = (hash >> 15) ^ hash;
  *first = hash % RAWOSD_FONT_MISSING_MAX;
  *second = (hash >> 17) % RAWOSD_FONT_MISSING_MAX;
}

static bool
font_missing(const RawosdRenderer *renderer, FcChar32 codepoint)
{
  size_t first;
  size_t second;

  missing_indexes(codepoint, &first, &second);
  return renderer->font.missing[first] == codepoint ||
         renderer->font.missing[second] == codepoint;
}

static void
remember_missing(RawosdRenderer *renderer, FcChar32 codepoint)
{
  size_t first;
  size_t second;

  missing_indexes(codepoint, &first, &second);
  renderer->font.missing[renderer->font.missing[first] ? second : first] =
    codepoint;
}

static XftFont *
font_for_char(RawosdRenderer *renderer, FcChar32 codepoint)
{
  FcCharSet *charset;
  FcPattern *match;
  FcPattern *pattern;
  FcResult result;
  XftFont *font;
  size_t i;

  if (XftCharExists(renderer->display, renderer->font.primary, codepoint))
    return renderer->font.primary;
  for (i = 0; i < renderer->font.fallback_count; ++i)
    if (XftCharExists(renderer->display, renderer->font.fallbacks[i], codepoint))
      return renderer->font.fallbacks[i];

  if (!renderer->font.pattern || font_missing(renderer, codepoint) ||
      renderer->font.fallback_count >= RAWOSD_FONT_FALLBACK_MAX)
    return renderer->font.primary;

  charset = FcCharSetCreate();
  pattern = FcPatternDuplicate(renderer->font.pattern);
  if (!charset || !pattern)
  {
    if (charset)
      FcCharSetDestroy(charset);
    if (pattern)
      FcPatternDestroy(pattern);
    remember_missing(renderer, codepoint);
    return renderer->font.primary;
  }

  FcCharSetAddChar(charset, codepoint);
  FcPatternDel(pattern, FC_CHARSET);
  FcPatternAddCharSet(pattern, FC_CHARSET, charset);
  FcPatternDel(pattern, FC_SCALABLE);
  FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
  FcPatternDel(pattern, FC_COLOR);
  FcPatternAddBool(pattern, FC_COLOR, FcFalse);
  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  match = XftFontMatch(renderer->display, renderer->screen, pattern, &result);
  FcPatternDestroy(pattern);
  FcCharSetDestroy(charset);
  if (!match)
  {
    remember_missing(renderer, codepoint);
    return renderer->font.primary;
  }
  font = XftFontOpenPattern(renderer->display, match);
  if (!font)
  {
    FcPatternDestroy(match);
    remember_missing(renderer, codepoint);
    return renderer->font.primary;
  }
  if (!XftCharExists(renderer->display, font, codepoint))
  {
    XftFontClose(renderer->display, font);
    remember_missing(renderer, codepoint);
    return renderer->font.primary;
  }
  renderer->font.fallbacks[renderer->font.fallback_count++] = font;
  return font;
}

static int
font_text_width(RawosdRenderer *renderer, XftFont *font,
                const char *text, size_t length)
{
  XGlyphInfo extents;

  if (!font || !text || !length)
    return 0;
  XftTextExtentsUtf8(renderer->display, font, (const FcChar8 *)text,
                     (int)length, &extents);
  return extents.xOff;
}

static int
text_width(RawosdRenderer *renderer, const char *text)
{
  size_t length = strlen(text);
  size_t position = 0;
  int width = 0;

  while (position < length)
  {
    FcChar32 codepoint;
    XftFont *font;
    size_t next;
    size_t start = position;

    next = utf8_decode(text + position, length - position, &codepoint);
    if (!next)
      break;
    font = font_for_char(renderer, codepoint);
    position += next;
    while (position < length)
    {
      XftFont *next_font;
      next = utf8_decode(text + position, length - position, &codepoint);
      if (!next)
        break;
      next_font = font_for_char(renderer, codepoint);
      if (next_font != font)
        break;
      position += next;
    }
    width += font_text_width(renderer, font, text + start, position - start);
  }
  return width;
}

static void
draw_utf8(RawosdRenderer *renderer, XftDraw *draw, XftColor *color,
          int x, int y, int height, const char *text)
{
  size_t length = strlen(text);
  size_t position = 0;

  while (position < length)
  {
    FcChar32 codepoint;
    XftFont *font;
    size_t next;
    size_t start = position;

    next = utf8_decode(text + position, length - position, &codepoint);
    if (!next)
      break;
    font = font_for_char(renderer, codepoint);
    position += next;
    while (position < length)
    {
      XftFont *next_font;
      next = utf8_decode(text + position, length - position, &codepoint);
      if (!next)
        break;
      next_font = font_for_char(renderer, codepoint);
      if (next_font != font)
        break;
      position += next;
    }
    {
      int font_height = font->ascent + font->descent;
      int baseline = y + (height - font_height) / 2 + font->ascent;
      XftDrawStringUtf8(draw, color, font, x, baseline,
                        (const FcChar8 *)text + start,
                        (int)(position - start));
    }
    x += font_text_width(renderer, font, text + start, position - start);
  }
}

static void
free_font(RawosdRenderer *renderer)
{
  size_t i;

  for (i = 0; i < renderer->font.fallback_count; ++i)
    XftFontClose(renderer->display, renderer->font.fallbacks[i]);
  if (renderer->font.primary)
    XftFontClose(renderer->display, renderer->font.primary);
  if (renderer->font.pattern)
    FcPatternDestroy(renderer->font.pattern);
  memset(&renderer->font, 0, sizeof(renderer->font));
}

static int
load_font(RawosdRenderer *renderer, const char *name, FILE *err)
{
  renderer->font.primary = XftFontOpenName(renderer->display,
                                            renderer->screen, name);
  if (!renderer->font.primary)
  {
    if (err)
      fprintf(err, "rawosd: cannot load font '%s'\n", name);
    return -1;
  }
  renderer->font.pattern = FcNameParse((const FcChar8 *)name);
  if (!renderer->font.pattern)
  {
    if (err)
      fprintf(err, "rawosd: cannot parse font pattern '%s'\n", name);
    free_font(renderer);
    return -1;
  }
  renderer->font.height =
    renderer->font.primary->ascent + renderer->font.primary->descent;
  return 0;
}

static void
free_colors(RawosdRenderer *renderer)
{
  size_t i;
  size_t j;
  Colormap cmap = DefaultColormap(renderer->display, renderer->screen);
  Visual *visual = DefaultVisual(renderer->display, renderer->screen);

  for (i = 0; i < RAWOSD_SCHEME_LAST; ++i)
    for (j = 0; j < RAWOSD_COLOR_LAST; ++j)
      if (renderer->color_allocated[i][j])
      {
        XftColorFree(renderer->display, visual, cmap,
                     &renderer->colors[i][j]);
        renderer->color_allocated[i][j] = false;
      }
}

static int
load_colors(RawosdRenderer *renderer, const RawosdConfig *config, FILE *err)
{
  size_t i;
  size_t j;
  Colormap cmap = DefaultColormap(renderer->display, renderer->screen);
  Visual *visual = DefaultVisual(renderer->display, renderer->screen);

  for (i = 0; i < RAWOSD_SCHEME_LAST; ++i)
    for (j = 0; j < RAWOSD_COLOR_LAST; ++j)
    {
      if (!XftColorAllocName(renderer->display, visual, cmap,
                             config->colors[i][j], &renderer->colors[i][j]))
      {
        if (err)
          fprintf(err, "rawosd: cannot allocate color '%s'\n",
                  config->colors[i][j]);
        free_colors(renderer);
        return -1;
      }
      renderer->color_allocated[i][j] = true;
    }
  return 0;
}

static void
set_window_properties(RawosdRenderer *renderer)
{
  XClassHint hint = { (char *)"rawosd", (char *)"rawosd" };
  Atom utf8 = XInternAtom(renderer->display, "UTF8_STRING", False);
  Atom net_name = XInternAtom(renderer->display, "_NET_WM_NAME", False);
  Atom net_type = XInternAtom(renderer->display, "_NET_WM_WINDOW_TYPE", False);
  Atom notification = XInternAtom(renderer->display,
                                  "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
  static const char name[] = "rawosd";

  XSetClassHint(renderer->display, renderer->window, &hint);
  XStoreName(renderer->display, renderer->window, name);
  XChangeProperty(renderer->display, renderer->window, net_name, utf8, 8,
                  PropModeReplace, (const unsigned char *)name,
                  (int)(sizeof(name) - 1u));
  XChangeProperty(renderer->display, renderer->window, net_type, XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)&notification, 1);
}

int
rawosd_renderer_init(RawosdRenderer *renderer, Display *display, int screen,
                     RawosdConfig *config, FILE *err)
{
  XSetWindowAttributes attributes;

  if (!renderer || !display || !config)
    return -1;
  memset(renderer, 0, sizeof(*renderer));
  renderer->display = display;
  renderer->screen = screen;
  renderer->root = RootWindow(display, screen);
  renderer->config = config;
  renderer->gc = XCreateGC(display, renderer->root, 0, NULL);
  if (!renderer->gc)
    return -1;
  if (load_font(renderer, config->font, err) < 0 ||
      load_colors(renderer, config, err) < 0)
  {
    rawosd_renderer_destroy(renderer);
    return -1;
  }

  memset(&attributes, 0, sizeof(attributes));
  attributes.override_redirect = True;
  attributes.background_pixel =
    renderer->colors[RAWOSD_SCHEME_SELECTED][RAWOSD_COLOR_BACKGROUND].pixel;
  attributes.event_mask = ExposureMask | VisibilityChangeMask;
  renderer->window = XCreateWindow(
    display, renderer->root, 0, 0, 1, 1, 0,
    DefaultDepth(display, screen), CopyFromParent,
    DefaultVisual(display, screen),
    CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);
  if (renderer->window == None)
  {
    if (err)
      fputs("rawosd: cannot create presentation window\n", err);
    rawosd_renderer_destroy(renderer);
    return -1;
  }
  set_window_properties(renderer);
  return 0;
}

void
rawosd_renderer_destroy(RawosdRenderer *renderer)
{
  if (!renderer || !renderer->display)
    return;
  if (renderer->pixmap != None)
    XFreePixmap(renderer->display, renderer->pixmap);
  if (renderer->window != None)
    XDestroyWindow(renderer->display, renderer->window);
  free_colors(renderer);
  free_font(renderer);
  if (renderer->gc)
    XFreeGC(renderer->display, renderer->gc);
  memset(renderer, 0, sizeof(*renderer));
}

int
rawosd_renderer_reconfigure(RawosdRenderer *renderer,
                            RawosdConfig *config, FILE *err)
{
  RawosdRenderer next;

  if (!renderer || !config)
    return -1;
  memset(&next, 0, sizeof(next));
  next.display = renderer->display;
  next.screen = renderer->screen;

  if (load_font(&next, config->font, err) < 0 ||
      load_colors(&next, config, err) < 0)
  {
    free_colors(&next);
    free_font(&next);
    return -1;
  }

  rawosd_renderer_hide(renderer);
  free_colors(renderer);
  free_font(renderer);
  renderer->font = next.font;
  memcpy(renderer->colors, next.colors, sizeof(renderer->colors));
  memcpy(renderer->color_allocated, next.color_allocated,
         sizeof(renderer->color_allocated));
  memset(&next.font, 0, sizeof(next.font));
  memset(next.color_allocated, 0, sizeof(next.color_allocated));
  renderer->config = config;
  XSetWindowBackground(renderer->display, renderer->window,
                       renderer->colors[RAWOSD_SCHEME_SELECTED]
                                       [RAWOSD_COLOR_BACKGROUND].pixel);
  return 0;
}

int
rawosd_renderer_show(RawosdRenderer *renderer,
                     const RawosdPresentation *presentation,
                     const RawosdState *state, FILE *err)
{
  const RawosdMonitorState *monitor;
  XftColor *colors;
  XftDraw *draw;
  unsigned int padding;
  unsigned int text_width_value;
  unsigned int marker_height;
  unsigned int marker_width;
  int text_x;
  int x;
  int y;

  if (!renderer || !presentation || !presentation->show || !state)
    return 0;
  monitor = rawosd_state_monitor(state, presentation->monitor);
  if (!monitor)
    return -1;
  colors = renderer->colors[presentation->scheme];
  padding = renderer->config->padding;
  text_width_value = (unsigned int)MAX(text_width(renderer, presentation->text), 1);
  renderer->width = MAX(text_width_value + 2u * padding,
                        renderer->config->min_width);
  renderer->height = (unsigned int)renderer->font.height + 2u * padding;
  if (renderer->width > (unsigned int)monitor->work_width)
    renderer->width = (unsigned int)monitor->work_width;
  if (renderer->height > (unsigned int)monitor->work_height)
    renderer->height = (unsigned int)monitor->work_height;

  x = monitor->work_x +
      (monitor->work_width - (int)renderer->width) / 2;
  if (renderer->config->position == RAWOSD_POSITION_TOP)
    y = monitor->work_y + (int)renderer->config->offset;
  else
    y = monitor->work_y + monitor->work_height - (int)renderer->height -
        (int)renderer->config->offset;
  if (y < monitor->work_y)
    y = monitor->work_y;
  if (y + (int)renderer->height > monitor->work_y + monitor->work_height)
    y = monitor->work_y + monitor->work_height - (int)renderer->height;

  if (renderer->pixmap != None)
    XFreePixmap(renderer->display, renderer->pixmap);
  renderer->pixmap = XCreatePixmap(renderer->display, renderer->root,
                                   renderer->width, renderer->height,
                                   DefaultDepth(renderer->display,
                                                renderer->screen));
  if (renderer->pixmap == None)
  {
    if (err)
      fputs("rawosd: cannot allocate presentation pixmap\n", err);
    return -1;
  }
  XSetForeground(renderer->display, renderer->gc,
                 colors[RAWOSD_COLOR_BACKGROUND].pixel);
  XFillRectangle(renderer->display, renderer->pixmap, renderer->gc,
                 0, 0, renderer->width, renderer->height);

  draw = XftDrawCreate(renderer->display, renderer->pixmap,
                       DefaultVisual(renderer->display, renderer->screen),
                       DefaultColormap(renderer->display, renderer->screen));
  if (!draw)
    return -1;
  text_x = renderer->width > text_width_value
             ? ((int)renderer->width - (int)text_width_value) / 2
             : 0;
  draw_utf8(renderer, draw, &colors[RAWOSD_COLOR_FOREGROUND], text_x,
            (int)padding, renderer->font.height, presentation->text);
  XftDrawDestroy(draw);

  marker_height = MIN(2u, renderer->height);
  marker_width = MIN(renderer->width / 3u, 64u);
  if (marker_height && marker_width)
  {
    XSetForeground(renderer->display, renderer->gc,
                   colors[RAWOSD_COLOR_ACCENT].pixel);
    XFillRectangle(renderer->display, renderer->pixmap, renderer->gc,
                   (renderer->width - marker_width) / 2u,
                   renderer->height - marker_height,
                   marker_width, marker_height);
  }

  XMoveResizeWindow(renderer->display, renderer->window, x, y,
                    renderer->width, renderer->height);
  rawosd_renderer_redraw(renderer);
  XMapRaised(renderer->display, renderer->window);
  XFlush(renderer->display);
  renderer->visible = true;
  renderer->deadline_ms = monotonic_ms() + renderer->config->duration_ms;
  return 0;
}

void
rawosd_renderer_hide(RawosdRenderer *renderer)
{
  if (!renderer || !renderer->visible || renderer->window == None)
    return;
  XUnmapWindow(renderer->display, renderer->window);
  XFlush(renderer->display);
  renderer->visible = false;
  renderer->deadline_ms = 0;
}

void
rawosd_renderer_expire(RawosdRenderer *renderer)
{
  if (renderer && renderer->visible && monotonic_ms() >= renderer->deadline_ms)
    rawosd_renderer_hide(renderer);
}

int
rawosd_renderer_timeout(const RawosdRenderer *renderer)
{
  uint64_t now;
  uint64_t remaining;

  if (!renderer || !renderer->visible)
    return -1;
  now = monotonic_ms();
  if (now >= renderer->deadline_ms)
    return 0;
  remaining = renderer->deadline_ms - now;
  return remaining > (uint64_t)INT_MAX ? INT_MAX : (int)remaining;
}

bool
rawosd_renderer_is_expose(const RawosdRenderer *renderer,
                           const XExposeEvent *event)
{
  return renderer && event && event->window == renderer->window &&
         renderer->visible;
}

bool
rawosd_renderer_handle_visibility(RawosdRenderer *renderer,
                                   const XVisibilityEvent *event)
{
  if (!renderer || !event || event->window != renderer->window ||
      !renderer->visible)
    return false;

  if (event->state != VisibilityUnobscured)
  {
    XRaiseWindow(renderer->display, renderer->window);
    XFlush(renderer->display);
  }
  return true;
}

void
rawosd_renderer_redraw(RawosdRenderer *renderer)
{
  if (!renderer || renderer->pixmap == None || renderer->window == None)
    return;
  XCopyArea(renderer->display, renderer->pixmap, renderer->window,
            renderer->gc, 0, 0, renderer->width, renderer->height, 0, 0);
}
