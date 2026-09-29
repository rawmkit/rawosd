project(
  'rawosd',
  'c',
  version: '0.1.0',
  meson_version: '>=1.6.0',
  default_options: [
    'c_std=c99',
    'warning_level=3',
    'sysconfdir=/etc',
  ],
)

add_project_arguments(
  '-D_DEFAULT_SOURCE',
  '-D_BSD_SOURCE',
  '-D_POSIX_C_SOURCE=200809L',
  language: 'c',
)

subdir('src')
subdir('tests')
