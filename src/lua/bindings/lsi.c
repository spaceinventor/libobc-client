#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <param/param.h>
#include "lsi.h"

extern param_t lua_state;

int get_timeout(lua_State * L) {

    lua_getglobal(L, "Timeout");
    int timeout = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return timeout;

}

static int si_lua_status(lua_State *L) {

    lua_getfield(L, LUA_REGISTRYINDEX, "__offset");
    int offset = lua_tointeger(L, -1);
    lua_pop(L, 1);

    uint32_t status = luaL_checkinteger(L, 1);
    param_set_uint32_array(&lua_state, offset, status);

    return 0;
}

LUAMOD_API int luaopen_si(lua_State * L, int task_offset) {

    lua_pushinteger(L, 1000);
    lua_setglobal(L, "Timeout");

    lua_pushcfunction(L, si_lua_status);
    lua_setglobal(L, "set_state");

    lua_pushinteger(L, task_offset);
    lua_setfield(L, LUA_REGISTRYINDEX, "__offset");


    return 1;
}