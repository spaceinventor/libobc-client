project('obc-client', ['c'], version: '0.0.1',
    subproject_dir: 'lib',
    default_options : [
        'c_std=gnu99',
        'default_library=static'
    ])
# Do not use c99 for c standard as it will produce a wrong build because
# functions like localtime_r will be not declared and C will assume they
# return an integer instead of a pointer and on 64bit this is horribly
# wrong.
# The gnu99 standard ensure that __USE_POSIX is defined (among other
# things) to get declaration of functions like localtime_r.

param_dep = dependency('param', fallback:['param', 'param_dep'])
csp_dep = dependency('csp', fallback:['csp', 'csp_dep'])
sources = []
dependencies = []
includes = []

subdir('src/lua')
dependencies += lua_dep
sc_utils_deps = [param_dep, csp_dep]
sc_utils_inc = ['include']
sc_utils_src = files(['src/scheduler/sc_utils.c',])
sc_utils_lib = library('sc_utils',
    sources: sc_utils_src,
    dependencies : sc_utils_deps,
    include_directories : sc_utils_inc + ['src/scheduler'],
)
sc_utils_dep = declare_dependency(dependencies: sc_utils_deps, include_directories: sc_utils_inc, link_with : sc_utils_lib)

sc_client_inc = 'include'
sc_client_src = files(['src/scheduler/sc_client.c',])
sc_client_lib = library('sc_client',
    sources: sc_client_src,
    dependencies : [sc_utils_dep],
    include_directories : sc_client_inc,
)
sc_client_dep = declare_dependency(dependencies: [sc_utils_dep], include_directories: sc_client_inc, link_with : sc_client_lib)

dependencies += sc_utils_dep
dependencies += sc_client_dep

obc_inc = include_directories('include', 'include/lua', '.')
obc_lib = static_library('obc',
    sources: sources,
    dependencies : dependencies,
    include_directories : obc_inc,
)

obcclient_dep = declare_dependency(dependencies: dependencies, include_directories: obc_inc, link_with : obc_lib)
