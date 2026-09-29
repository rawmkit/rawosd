#include "rawosd-config.h"

#include <libconfig.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

static const char *const scheme_names[RAWOSD_SCHEME_LAST] = {
  "normal",
  "floating",
};

static const char *const color_names[RAWOSD_COLOR_LAST] = {
  "foreground",
  "background",
  "accent",
};

static const char *const layout_names[6] = {
  "tile",
  "floating",
  "monocle",
  "bstack",
  "bstack_horizontal",
  "grid",
};

static void
replace_string(char **target, const char *value)
{
  char *copy = strdup(value ? value : "");

  if (!copy)
  {
    fputs("rawosd: out of memory\n", stderr);
    exit(EXIT_FAILURE);
  }
  free(*target);
  *target = copy;
}

static int
fail(FILE *err, const char *path, const char *message)
{
  if (err)
    fprintf(err, "rawosd: configuration error%s%s%s: %s\n",
            path ? " at " : "", path ? path : "", path ? "" : "", message);
  return -1;
}

static int
known_name(config_setting_t *group, int index, const char *const *names,
           size_t count, FILE *err, const char *path)
{
  const char *name = config_setting_name(config_setting_get_elem(group, index));
  size_t i;

  for (i = 0; i < count; ++i)
    if (name && strcmp(name, names[i]) == 0)
      return 0;
  return fail(err, path, "unknown setting");
}

static int
check_group_names(config_setting_t *group, const char *const *names,
                  size_t count, FILE *err, const char *path)
{
  int i;
  int length;

  if (!group)
    return 0;
  if (config_setting_type(group) != CONFIG_TYPE_GROUP)
    return fail(err, path, "expected group");
  length = config_setting_length(group);
  for (i = 0; i < length; ++i)
    if (known_name(group, i, names, count, err, path) < 0)
      return -1;
  return 0;
}

static config_setting_t *
lookup_path(config_setting_t *root, const char *path)
{
  char *copy;
  char *save = NULL;
  char *part;
  config_setting_t *setting = root;

  copy = strdup(path);
  if (!copy)
    return NULL;
  part = strtok_r(copy, ".", &save);
  while (part && setting)
  {
    setting = config_setting_lookup(setting, part);
    part = strtok_r(NULL, ".", &save);
  }
  free(copy);
  return setting;
}

static int
read_string(config_setting_t *root, const char *path, char **target, FILE *err)
{
  config_setting_t *setting = lookup_path(root, path);
  const char *value;

  if (!setting)
    return 0;
  if (config_setting_type(setting) != CONFIG_TYPE_STRING)
    return fail(err, path, "expected string");
  value = config_setting_get_string(setting);
  if (!value)
    return fail(err, path, "invalid string");
  replace_string(target, value);
  return 0;
}

static int
read_bool(config_setting_t *root, const char *path, bool *target, FILE *err)
{
  config_setting_t *setting = lookup_path(root, path);

  if (!setting)
    return 0;
  if (config_setting_type(setting) != CONFIG_TYPE_BOOL)
    return fail(err, path, "expected boolean");
  *target = config_setting_get_bool(setting) != 0;
  return 0;
}

static int
read_uint(config_setting_t *root, const char *path, unsigned int *target,
          FILE *err)
{
  config_setting_t *setting = lookup_path(root, path);
  int value;

  if (!setting)
    return 0;
  if (config_setting_type(setting) != CONFIG_TYPE_INT)
    return fail(err, path, "expected integer");
  value = config_setting_get_int(setting);
  if (value < 0)
    return fail(err, path, "expected non-negative integer");
  *target = (unsigned int)value;
  return 0;
}

static int
apply_config(config_t *source, RawosdConfig *config, FILE *err)
{
  static const char *const root_names[] = {
    "font", "position", "geometry", "duration_ms", "padding", "offset",
    "min_width", "colors", "layouts",
  };
  static const char *const color_group_names[] = { "normal", "floating" };
  static const char *const color_component_names[] = {
    "foreground", "background", "accent",
  };
  config_setting_t *root = config_root_setting(source);
  config_setting_t *colors;
  config_setting_t *layouts;
  const char *position;
  size_t i;
  size_t j;

  if (check_group_names(root, root_names,
                        sizeof(root_names) / sizeof(root_names[0]),
                        err, NULL) < 0)
    return -1;

  if (read_string(root, "font", &config->font, err) < 0 ||
      read_bool(root, "geometry", &config->geometry, err) < 0 ||
      read_uint(root, "duration_ms", &config->duration_ms, err) < 0 ||
      read_uint(root, "padding", &config->padding, err) < 0 ||
      read_uint(root, "offset", &config->offset, err) < 0 ||
      read_uint(root, "min_width", &config->min_width, err) < 0)
    return -1;

  if (config_lookup_string(source, "position", &position))
  {
    if (strcmp(position, "top") == 0)
      config->position = RAWOSD_POSITION_TOP;
    else if (strcmp(position, "bottom") == 0)
      config->position = RAWOSD_POSITION_BOTTOM;
    else
      return fail(err, "position", "expected \"top\" or \"bottom\"");
  }
  else if (config_setting_lookup(root, "position") != NULL)
    return fail(err, "position", "expected string");

  colors = config_setting_lookup(root, "colors");
  if (colors)
  {
    if (check_group_names(colors, color_group_names,
                          sizeof(color_group_names) /
                            sizeof(color_group_names[0]),
                          err, "colors") < 0)
      return -1;
    for (i = 0; i < RAWOSD_SCHEME_LAST; ++i)
    {
      config_setting_t *scheme = config_setting_lookup(colors, scheme_names[i]);
      if (!scheme)
        continue;
      if (check_group_names(scheme, color_component_names,
                            sizeof(color_component_names) /
                              sizeof(color_component_names[0]),
                            err, scheme_names[i]) < 0)
        return -1;
      for (j = 0; j < RAWOSD_COLOR_LAST; ++j)
      {
        char path[96];
        int n = snprintf(path, sizeof(path), "colors.%s.%s",
                         scheme_names[i], color_names[j]);
        if (n < 0 || (size_t)n >= sizeof(path) ||
            read_string(root, path, &config->colors[i][j], err) < 0)
          return -1;
      }
    }
  }

  layouts = config_setting_lookup(root, "layouts");
  if (layouts)
  {
    if (check_group_names(layouts, layout_names, 6, err, "layouts") < 0)
      return -1;
    for (i = 0; i < 6; ++i)
    {
      char path[96];
      int n = snprintf(path, sizeof(path), "layouts.%s", layout_names[i]);
      if (n < 0 || (size_t)n >= sizeof(path) ||
          read_string(root, path, &config->layout_symbols[i], err) < 0)
        return -1;
    }
  }
  return 0;
}

static int
load_file(RawosdConfig *config, const char *path, bool required, FILE *err)
{
  config_t source;
  int rc = 0;

  if (!required && access(path, R_OK) != 0)
    return errno == ENOENT ? 0 : fail(err, path, strerror(errno));

  config_init(&source);
  if (!config_read_file(&source, path))
  {
    if (err)
      fprintf(err, "rawosd: %s:%d: %s\n", path,
              config_error_line(&source), config_error_text(&source));
    config_destroy(&source);
    return -1;
  }
  rc = apply_config(&source, config, err);
  config_destroy(&source);
  return rc;
}

static char *
user_config_path(void)
{
  const char *xdg = getenv("XDG_CONFIG_HOME");
  const char *home;
  size_t length;
  char *path;

  if (xdg && *xdg && xdg[0] == '/')
  {
    length = strlen(xdg) + strlen("/rawosd/rawosd.conf") + 1u;
    path = malloc(length);
    if (!path)
      return NULL;
    snprintf(path, length, "%s/rawosd/rawosd.conf", xdg);
    return path;
  }
  home = getenv("HOME");
  if (!home || !*home)
    return NULL;
  length = strlen(home) + strlen("/.config/rawosd/rawosd.conf") + 1u;
  path = malloc(length);
  if (!path)
    return NULL;
  snprintf(path, length, "%s/.config/rawosd/rawosd.conf", home);
  return path;
}

void
rawosd_config_init(RawosdConfig *config)
{
  static const char *const default_colors[RAWOSD_SCHEME_LAST][RAWOSD_COLOR_LAST] = {
    { "#ffffff", "#0066ff", "#0066ff" },
    { "#ffffff", "#0066ff", "#0066ff" },
  };
  static const char *const default_symbols[6] = {
    "[]=", "<1/1>", "[1/1]", "TTT", "===", "###",
  };
  size_t i;
  size_t j;

  memset(config, 0, sizeof(*config));
  config->font = strdup("monospace:size=9");
  config->position = RAWOSD_POSITION_BOTTOM;
  config->geometry = false;
  config->duration_ms = 800;
  config->padding = 10;
  config->offset = 24;
  config->min_width = 0;
  if (!config->font)
    goto oom;
  for (i = 0; i < RAWOSD_SCHEME_LAST; ++i)
    for (j = 0; j < RAWOSD_COLOR_LAST; ++j)
    {
      config->colors[i][j] = strdup(default_colors[i][j]);
      if (!config->colors[i][j])
        goto oom;
    }
  for (i = 0; i < 6; ++i)
  {
    config->layout_symbols[i] = strdup(default_symbols[i]);
    if (!config->layout_symbols[i])
      goto oom;
  }
  return;

oom:
  fputs("rawosd: out of memory\n", stderr);
  rawosd_config_destroy(config);
  exit(EXIT_FAILURE);
}

void
rawosd_config_destroy(RawosdConfig *config)
{
  size_t i;
  size_t j;

  if (!config)
    return;
  free(config->font);
  for (i = 0; i < RAWOSD_SCHEME_LAST; ++i)
    for (j = 0; j < RAWOSD_COLOR_LAST; ++j)
      free(config->colors[i][j]);
  for (i = 0; i < 6; ++i)
    free(config->layout_symbols[i]);
  memset(config, 0, sizeof(*config));
}

int
rawosd_config_validate(const RawosdConfig *config, FILE *err)
{
  size_t i;
  size_t j;

  if (!config->font || !config->font[0])
    return fail(err, "font", "must not be empty");
  if (config->duration_ms < 100 || config->duration_ms > 5000)
    return fail(err, "duration_ms", "must be between 100 and 5000");
  if (config->padding > 256)
    return fail(err, "padding", "must not exceed 256");
  if (config->offset > 4096)
    return fail(err, "offset", "must not exceed 4096");
  if (config->min_width > 8192)
    return fail(err, "min_width", "must not exceed 8192");
  for (i = 0; i < RAWOSD_SCHEME_LAST; ++i)
    for (j = 0; j < RAWOSD_COLOR_LAST; ++j)
      if (!config->colors[i][j] || !config->colors[i][j][0])
        return fail(err, "colors", "color strings must not be empty");
  for (i = 0; i < 6; ++i)
    if (!config->layout_symbols[i] || strlen(config->layout_symbols[i]) > 32)
      return fail(err, "layouts", "layout symbols must be at most 32 bytes");
  return 0;
}

int
rawosd_config_load(RawosdConfig *config, const char *explicit_path, FILE *err)
{
  char system_path[PATH_MAX];
  char *user_path;

  if (explicit_path)
  {
    if (load_file(config, explicit_path, true, err) < 0)
      return -1;
    return rawosd_config_validate(config, err);
  }

  snprintf(system_path, sizeof(system_path), "%s/rawosd/rawosd.conf", SYSCONFDIR);
  if (load_file(config, system_path, false, err) < 0)
    return -1;
  user_path = user_config_path();
  if (user_path)
  {
    int rc = load_file(config, user_path, false, err);
    free(user_path);
    if (rc < 0)
      return -1;
  }
  return rawosd_config_validate(config, err);
}
