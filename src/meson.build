rawosd_protocol_source = files('rawosd-protocol.c')
rawosd_config_source = files('rawosd-config.c')
rawosd_present_source = files('rawosd-present.c')
rawosd_control_source = files('rawosd-control.c')
rawosd_render_source = files('rawosd-render.c')
rawosd_src_include = include_directories('.')

rawosd = executable(
  'rawosd',
  'rawosd.c',
  rawosd_protocol_source,
  rawosd_config_source,
  rawosd_present_source,
  rawosd_control_source,
  rawosd_render_source,
  include_directories: rawosd_src_include,
  dependencies: [x11_dep, xft_dep, fontconfig_dep, libconfig_dep],
  c_args: '-DVERSION="@0@"'.format(meson.project_version()),
  install: true,
)
