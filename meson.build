project('obc-client', ['c'], version: '0.0.1',
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

# sources += files(['src/scheduler/sc_utils.c',])
sources += files(['src/scheduler/sc_client.c',])

obc_lib = static_library('obc-client',
    sources: sources,
    dependencies : dependencies,
    include_directories : include_directories('include', '.'),
)

obc_client_dep = declare_dependency(dependencies: dependencies, include_directories: includes, link_with : obc_lib)
