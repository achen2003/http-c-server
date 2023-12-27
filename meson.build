project('http-c-server', 'c',
  version : '0.1.0',
  default_options : ['c_std=c17', 'warning_level=3', 'c_args=-D_DEFAULT_SOURCE']
)

jansson_dep = dependency('jansson', 
                          version: '>=2.14')

executable('client',
           'client_1.c')

executable('server',
           'server_1.c',
           dependencies: jansson_dep)
