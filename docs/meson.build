scdoc = find_program('scdoc')

custom_target(
  'rawosd.1',
  input: 'rawosd.1.scdoc',
  output: 'rawosd.1',
  command: [scdoc],
  feed: true,
  capture: true,
  install: true,
  install_dir: join_paths(get_option('mandir'), 'man1'),
)

custom_target(
  'rawosd.conf.5',
  input: 'rawosd.conf.5.scdoc',
  output: 'rawosd.conf.5',
  command: [scdoc],
  feed: true,
  capture: true,
  install: true,
  install_dir: join_paths(get_option('mandir'), 'man5'),
)

install_data(
  'protocol.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'rawosd'),
)
