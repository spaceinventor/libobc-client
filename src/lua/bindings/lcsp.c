#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <csp/csp.h>
#include <csp/csp_hooks.h>
#include <csp/csp_types.h>
#include "lsi.h"


static int lua_get_time(lua_State *L) {

    csp_timestamp_t time;
    csp_clock_get_time(&time);

    lua_pushinteger(L, (time.tv_sec * 1E9 + time.tv_nsec) / 1E6);

    return 1;
}

static int lua_get_uptime(lua_State *L) {

    uint16_t node = luaL_checkinteger(L, 1);
    uint32_t uptime = 0;
    csp_get_uptime(node, get_timeout(L), &uptime);

    lua_pushinteger(L, uptime);

    return 1;
}

static int lua_ping(lua_State *L) {

    uint16_t node = luaL_checkinteger(L, 1);
    int size = luaL_optinteger(L, 2, 0);

    lua_pushinteger(L, csp_ping(node, get_timeout(L), size, CSP_O_CRC32));

    return 1;
}

static int lua_reboot(lua_State *L) {

    uint16_t node = luaL_checkinteger(L, 1);

    csp_reboot(node);

    return 0;
}

LUAMOD_API int luaopen_csp(lua_State * L) {

    lua_pushcfunction(L, lua_get_time);
    lua_setglobal(L, "get_time");

    lua_pushcfunction(L, lua_get_uptime);
    lua_setglobal(L, "uptime");

    lua_pushcfunction(L, lua_ping);
    lua_setglobal(L, "ping");

    lua_pushcfunction(L, lua_reboot);
    lua_setglobal(L, "reboot");

    return 1;
}