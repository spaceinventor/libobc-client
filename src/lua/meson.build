if get_option('freertos')
    dependencies += dependency('freertos', fallback: ['freertos', 'freertos_dep'])
endif

lua_deps = []
lua_cflags = ['-DLUA_COMPAT_5_3']
lua_shared_cflags = []
lua_library_name = 'lua_sub'
lua_library_prefix = 'lib'
lua_deps += meson.get_compiler('c').find_library('m', required : false)

lua_core_src = [ 
    'lapi.c',
    'lctype.c',
    'ldebug.c',
    'ldo.c',
    'lfunc.c',
    'lgc.c',
    'lmem.c',
    'lobject.c',
    'lopcodes.c',
    'lstate.c',
    'lstring.c',
    'ltable.c',
    'ltm.c',
    'lundump.c',
    'lvm.c',
    'lzio.c'
]

lua_lib_src = [
    'lauxlib.c',
    'lbaselib.c',
    'lcorolib.c',
    #'ldblib.c',
    # 'liolib.c',
    'lmathlib.c',
    # 'loadlib.c', 
    #'loslib.c', 
    'lstrlib.c', 
    'ltablib.c', 
    # 'lutf8lib.c', 
    'linit.c'
]

lua_param_src = [
    'bindings/lparam.c',
    'bindings/lcsp.c',
    'bindings/lsi.c'
]

if get_option('freertos') == true
	lua_cflags += '-DLUA_FREERTOS'
	lua_core_src += files([
        'bindings/lfreertos.c',
        'bindings/ltask.c',
	])
endif

if get_option('parser') == true
	lua_cflags += '-DLUA_PARSER'
	lua_core_src += files([
        'bindings/llinux.c',
        'lcode.c',
        'llex.c',
        'lparser.c',
        'ldump.c',
	])
endif

lua_tasks = get_option('lua_tasks')
lua_conf = configuration_data()
lua_conf.set('LUA_TASKS', lua_tasks)
configure_file(output: 'lua_conf.h', configuration: lua_conf)

lua_deps += dependency('csp', fallback: ['csp', 'csp_dep'], required: false).partial_dependency(links: false, includes: true)
lua_deps += dependency('param', fallback: ['param', 'param_dep'], required: false).partial_dependency(links: false, includes: true)

lua_include_dir = include_directories(['.', 'bindings', '../../include', '../../include/lua', '../../include/lua/bindings'])

liblua = static_library(lua_library_name,
    lua_core_src + lua_lib_src + lua_param_src,
    include_directories : lua_include_dir,
    dependencies: lua_deps,
    c_args: lua_cflags,
    install: true,
)
liblua_static = liblua

#param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()

lua_dep = declare_dependency(
    dependencies : lua_deps,
    link_with : liblua,
)
