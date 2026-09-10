project('obc-client', ['c'], version: '0.0.3',
    subproject_dir: 'lib',
    default_options : [
        'warning_level=2',
    ])
param_dep = dependency('param', fallback:['param', 'param_dep'])
csp_dep = dependency('csp', fallback:['csp', 'csp_dep'])

sources = []
dependencies = []
includes = include_directories('include')
dependencies += [param_dep, csp_dep]

sources += files([
    'src/hk/hk_client.c',
    'src/scheduler/sc_client.c',
    ]
)

install_subdir('include', install_dir : '.', install_tag: 'devel')

obc_lib = library('obc-client',
    sources: sources,
    dependencies : dependencies,
    include_directories : include_directories('include', '.'),
    install: true,
    soversion: meson.project_version()
)

pkg = import('pkgconfig')
pkg.generate(
	obc_lib,
	subdirs: ['.'],
    requires: ['param', 'csp']
)
obc_client_dep = declare_dependency(dependencies: dependencies, include_directories: includes, link_with : obc_lib)
