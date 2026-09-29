#ifndef RAWOSD_PRESENT_H
#define RAWOSD_PRESENT_H

#include <stdbool.h>
#include <stddef.h>

#include "rawosd-config.h"
#include "rawosd-protocol.h"

typedef struct {
  bool show;
  int monitor;
  RawosdScheme scheme;
  char text[512];
} RawosdPresentation;

int rawosd_present_format(const RawosdFeedback *feedback,
                          const RawosdState *state,
                          const RawosdConfig *config,
                          RawosdPresentation *presentation,
                          FILE *err);

#endif
