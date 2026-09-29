#include "rawosd-config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
fail(const char *message)
{
  fprintf(stderr, "config defaults test failed: %s\n", message);
  exit(EXIT_FAILURE);
}

static void
expect(int condition, const char *message)
{
  if (!condition)
    fail(message);
}

int
main(int argc, char **argv)
{
  RawosdConfig defaults;
  RawosdConfig loaded;
  size_t i;
  size_t j;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s RAWOSD.CONF\n", argv[0]);
    return EXIT_FAILURE;
  }
  rawosd_config_init(&defaults);
  rawosd_config_init(&loaded);
  if (rawosd_config_load(&loaded, argv[1], stderr) < 0)
    fail("canonical configuration did not load");

  expect(strcmp(defaults.font, loaded.font) == 0, "font differs");
  expect(defaults.position == loaded.position, "position differs");
  expect(defaults.geometry == loaded.geometry, "geometry differs");
  expect(defaults.duration_ms == loaded.duration_ms, "duration differs");
  expect(defaults.padding == loaded.padding, "padding differs");
  expect(defaults.offset == loaded.offset, "offset differs");
  expect(defaults.min_width == loaded.min_width, "minimum width differs");
  for (i = 0; i < RAWOSD_SCHEME_LAST; ++i)
    for (j = 0; j < RAWOSD_COLOR_LAST; ++j)
      expect(strcmp(defaults.colors[i][j], loaded.colors[i][j]) == 0,
             "color differs");
  for (i = 0; i < 6; ++i)
    expect(strcmp(defaults.layout_symbols[i], loaded.layout_symbols[i]) == 0,
           "layout symbol differs");

  rawosd_config_destroy(&loaded);
  rawosd_config_destroy(&defaults);
  puts("config defaults tests: ok");
  return EXIT_SUCCESS;
}
