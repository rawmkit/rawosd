#ifndef RAWOSD_CONFIG_H
#define RAWOSD_CONFIG_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
  RAWOSD_POSITION_TOP,
  RAWOSD_POSITION_BOTTOM
} RawosdPosition;

typedef enum {
  RAWOSD_SCHEME_NORMAL,
  RAWOSD_SCHEME_FLOATING,
  RAWOSD_SCHEME_LAST
} RawosdScheme;

typedef enum {
  RAWOSD_COLOR_FOREGROUND,
  RAWOSD_COLOR_BACKGROUND,
  RAWOSD_COLOR_ACCENT,
  RAWOSD_COLOR_LAST
} RawosdColor;

typedef struct {
  char *font;
  RawosdPosition position;
  bool geometry;
  unsigned int duration_ms;
  unsigned int padding;
  unsigned int offset;
  unsigned int min_width;
  char *colors[RAWOSD_SCHEME_LAST][RAWOSD_COLOR_LAST];
  char *layout_symbols[6];
} RawosdConfig;

void rawosd_config_init(RawosdConfig *config);
void rawosd_config_destroy(RawosdConfig *config);
int rawosd_config_load(RawosdConfig *config, const char *explicit_path,
                       FILE *err);
int rawosd_config_validate(const RawosdConfig *config, FILE *err);

#endif
