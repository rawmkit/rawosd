protocol_test = executable(
  'protocol-test',
  'protocol-test.c',
  rawosd_protocol_source,
  include_directories: rawosd_src_include,
  build_by_default: false,
)

test('protocol', protocol_test)

present_test = executable(
  'present-test',
  'present-test.c',
  rawosd_protocol_source,
  rawosd_config_source,
  rawosd_present_source,
  include_directories: rawosd_src_include,
  dependencies: libconfig_dep,
  build_by_default: false,
)

test('present', present_test)

control_owner_test_bin = executable(
  'control-owner-test',
  'control-owner-test.c',
  rawosd_protocol_source,
  rawosd_control_source,
  include_directories: rawosd_src_include,
  dependencies: x11_dep,
  build_by_default: false,
)

control_owner_test = find_program('control-owner.sh')
xvfb_program = find_program('Xvfb', required: false, native: true)
xwininfo_program = find_program('xwininfo', required: false, native: true)
if xvfb_program.found() and xwininfo_program.found()
  test(
    'control-owner',
    control_owner_test,
    args: [control_owner_test_bin],
    env: [
      'RAWOSD_TEST_XVFB=' + xvfb_program.full_path(),
      'RAWOSD_TEST_XWININFO=' + xwininfo_program.full_path(),
    ],
    workdir: meson.project_source_root(),
  )
endif

config_defaults_test = executable(
  'config-defaults-test',
  'config-defaults-test.c',
  rawosd_config_source,
  include_directories: rawosd_src_include,
  dependencies: libconfig_dep,
  build_by_default: false,
)

config_probe = executable(
  'config-probe',
  'config-probe.c',
  rawosd_config_source,
  include_directories: rawosd_src_include,
  dependencies: libconfig_dep,
  build_by_default: false,
)

test(
  'config-defaults',
  config_defaults_test,
  args: [join_paths(meson.project_source_root(), 'config', 'rawosd.conf')],
)

config_test = find_program('config.sh')
test(
  'config',
  config_test,
  args: [config_probe],
  workdir: meson.project_source_root(),
)

fake_rawm = executable(
  'fake-rawm',
  'fake-rawm.c',
  dependencies: x11_dep,
  build_by_default: false,
)

window_probe = executable(
  'window-probe',
  'window-probe.c',
  dependencies: x11_dep,
  build_by_default: false,
)

renderer_test = find_program('renderer.sh')
if xvfb_program.found() and xwininfo_program.found()
  test(
    'renderer',
    renderer_test,
    args: [
      rawosd,
      fake_rawm,
      window_probe,
      join_paths(meson.project_source_root(), 'tests', 'renderer.conf'),
    ],
    env: [
      'RAWOSD_TEST_XVFB=' + xvfb_program.full_path(),
      'RAWOSD_TEST_XWININFO=' + xwininfo_program.full_path(),
    ],
    workdir: meson.project_source_root(),
    timeout: 15,
  )
endif
