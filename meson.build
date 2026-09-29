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

x11_dep = dependency('x11')
xft_dep = dependency('xft')
fontconfig_dep = dependency('fontconfig')
libconfig_dep = dependency('libconfig')

sysconfdir = get_option('sysconfdir')
runtime_sysconfdir = sysconfdir
if not runtime_sysconfdir.startswith('/')
  runtime_sysconfdir = join_paths(get_option('prefix'), runtime_sysconfdir)
endif
add_project_arguments(
  '-DSYSCONFDIR="@0@"'.format(runtime_sysconfdir),
  language: 'c',
)

subdir('src')
subdir('config')
subdir('docs')
subdir('tests')

install_data(
  'README.md',
  'NEWS.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'rawosd'),
)
