#include "rawosd-config.h"

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
  RawosdConfig config;
  int rc;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s CONFIG\n", argv[0]);
    return EXIT_FAILURE;
  }
  rawosd_config_init(&config);
  rc = rawosd_config_load(&config, argv[1], stderr);
  rawosd_config_destroy(&config);
  return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
