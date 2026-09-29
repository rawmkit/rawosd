protocol_test = executable(
  'protocol-test',
  'protocol-test.c',
  rawosd_protocol_source,
  include_directories: rawosd_src_include,
  build_by_default: false,
)

test('protocol', protocol_test)
