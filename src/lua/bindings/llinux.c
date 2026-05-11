#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <unistd.h>

static int lua_sleep(lua_State *L) {
    // Check if the argument is a number and get it
    int delay = luaL_checkinteger(L, 1);

    // Call the FreeRTOS vTaskDelay function
    usleep(delay * 1000); // Assuming delay is in milliseconds

    return 0;
}

LUAMOD_API int luaopen_linux(lua_State * L) {
    lua_pushcfunction(L, lua_sleep);
    lua_setglobal(L, "sleep");
    return 1;
}