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
