# Copyright (c) 2025, Postgres Professional

# Does not support the PGXS infrastructure at this time. Please, compile as part
# of the contrib source tree.

jsquery_sources = files(
  'jsonb_gin_ops.c',
  'jsquery_constr.c',
  'jsquery_extract.c',
  'jsquery_io.c',
  'jsquery_op.c',
  'jsquery_support.c',
)

jsquery_gram = custom_target('jsquerygram',
  input: 'jsquery_gram.y',
  kwargs: bison_kw,
)
generated_sources += jsquery_gram.to_list()
jsquery_sources += jsquery_gram

run_command(flex, '--outfile=jsquery_scan.c', 'jsquery_scan.l', check: true)

if host_system == 'windows'
  jsquery_sources += rc_lib_gen.process(win32ver_rc, extra_args: [
    '--NAME', 'jsquery',
    '--FILEDESC', 'jsquery -  language to query jsonb data type.',])
endif

jsquery = shared_module('jsquery',
  jsquery_sources,
  include_directories: include_directories('.'),
  kwargs: contrib_mod_args,
)
contrib_targets += jsquery

install_data(
  'jsquery.control',
  'jsquery--1.0--1.1.sql',
  'jsquery--1.1.sql',
  kwargs: contrib_data_args,
)

tests += {
  'name': 'jsquery',
  'sd': meson.current_source_dir(),
  'bd': meson.current_build_dir(),
  'regress': {
    'sql': [
      'jsquery',
    ],
  },
}
